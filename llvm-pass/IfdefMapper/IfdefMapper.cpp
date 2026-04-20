// IfdefMapper.cpp — Clang plugin that records every preprocessor conditional
// block (#ifdef/#ifndef/#if) with its source location and macro condition.
// Outputs ast_mapping.json per-compilation.
//
// Build:  bash build.sh
// Invoke: clang++ \
//           -Xclang -load -Xclang ./IfdefMapper.so \
//           -Xclang -add-plugin -Xclang ifdef-mapper \
//           [-Xclang -plugin-arg-ifdef-mapper -Xclang output=<path>] \
//           -fsyntax-only <source.cpp>

#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/Token.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <map>
#include <stack>
#include <string>
#include <vector>

namespace {

// ─── data model ──────────────────────────────────────────────────────────────

struct Branch {
    std::string kind;        // "ifdef" | "ifndef" | "if" | "elif" | "else"
    std::string condition;
    unsigned    start_line;
    unsigned    end_line = 0;
};

struct Block {
    std::string         file;
    std::string         top_condition;
    std::string         top_kind;
    unsigned            start_line;
    unsigned            end_line = 0;
    std::vector<Branch> branches;
};

// ─── JSON helpers (stdlib-only) ───────────────────────────────────────────────

static std::string jsonEscape(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:   out += c;      break;
        }
    }
    return out;
}

static void writeJSON(const std::string &outPath,
                      const std::map<std::string, std::vector<Block>> &fileBlocks) {
    std::ofstream f(outPath);
    if (!f) {
        llvm::errs() << "[IfdefMapper] ERROR: cannot open output file: "
                     << outPath << "\n";
        return;
    }

    f << "{\n";
    f << "  \"tool\": \"IfdefMapper\",\n";
    f << "  \"files\": {\n";

    bool firstFile = true;
    for (const auto &[file, blocks] : fileBlocks) {
        if (!firstFile) f << ",\n";
        firstFile = false;

        f << "    \"" << jsonEscape(file) << "\": [\n";
        bool firstBlock = true;
        for (const auto &blk : blocks) {
            if (!firstBlock) f << ",\n";
            firstBlock = false;

            f << "      {\n";
            f << "        \"kind\": \""      << jsonEscape(blk.top_kind)      << "\",\n";
            f << "        \"condition\": \"" << jsonEscape(blk.top_condition) << "\",\n";
            f << "        \"start_line\": "  << blk.start_line               << ",\n";
            f << "        \"end_line\": "    << blk.end_line                 << ",\n";
            f << "        \"branches\": [\n";

            bool firstBranch = true;
            for (const auto &br : blk.branches) {
                if (!firstBranch) f << ",\n";
                firstBranch = false;
                f << "          {\n";
                f << "            \"kind\": \""      << jsonEscape(br.kind)      << "\",\n";
                f << "            \"condition\": \"" << jsonEscape(br.condition) << "\",\n";
                f << "            \"start_line\": "  << br.start_line            << ",\n";
                f << "            \"end_line\": "    << br.end_line              << "\n";
                f << "          }";
            }
            if (!blk.branches.empty()) f << "\n";
            f << "        ]\n";
            f << "      }";
        }
        if (!blocks.empty()) f << "\n";
        f << "    ]";
    }
    if (!fileBlocks.empty()) f << "\n";

    f << "  }\n";
    f << "}\n";
    f.flush();

    llvm::errs() << "[IfdefMapper] Wrote " << fileBlocks.size()
                 << " file(s) → " << outPath << "\n";
}

// ─── PPCallbacks implementation ───────────────────────────────────────────────

class IfdefMapperCallbacks : public clang::PPCallbacks {
public:
    IfdefMapperCallbacks(clang::SourceManager &SM)
        : SM(SM) {}

    // Called from the plugin action's EndSourceFileAction — guaranteed to fire
    // while both the Preprocessor and the DSO are still alive.
    void flush(const std::string &outPath) {
        writeJSON(outPath, fileBlocks);
    }

    // ── preprocessor hooks ────────────────────────────────────────────────────

    void Ifdef(clang::SourceLocation Loc,
               const clang::Token &MacroNameTok,
               const clang::MacroDefinition &) override {
        if (shouldSkip(Loc)) { pushSentinel(); return; }
        pushBlock(Loc, "ifdef",
                  MacroNameTok.getIdentifierInfo()->getName().str());
    }

    void Ifndef(clang::SourceLocation Loc,
                const clang::Token &MacroNameTok,
                const clang::MacroDefinition &) override {
        if (shouldSkip(Loc)) { pushSentinel(); return; }
        pushBlock(Loc, "ifndef",
                  MacroNameTok.getIdentifierInfo()->getName().str());
    }

    void If(clang::SourceLocation Loc,
            clang::SourceRange ConditionRange,
            ConditionValueKind) override {
        if (shouldSkip(Loc)) { pushSentinel(); return; }
        pushBlock(Loc, "if", extractRangeText(ConditionRange));
    }

    void Elif(clang::SourceLocation Loc,
              clang::SourceRange ConditionRange,
              ConditionValueKind,
              clang::SourceLocation) override {
        if (stack.empty() || stack.top().sentinel) return;
        closeTopBranch(Loc);
        openBranch(Loc, "elif", extractRangeText(ConditionRange));
    }

    void Else(clang::SourceLocation Loc,
              clang::SourceLocation) override {
        if (stack.empty() || stack.top().sentinel) return;
        closeTopBranch(Loc);
        openBranch(Loc, "else", "");
    }

    void Endif(clang::SourceLocation Loc,
               clang::SourceLocation) override {
        if (stack.empty()) return;
        if (!stack.top().sentinel) {
            closeTopBranch(Loc);
            popBlock(Loc);
        } else {
            stack.pop();
        }
    }

private:
    clang::SourceManager &SM;

    struct StackFrame {
        Block  blk;
        Branch currentBranch;
        bool   sentinel = false;  // true for system-header / skipped blocks
    };
    std::stack<StackFrame>                    stack;
    std::map<std::string, std::vector<Block>> fileBlocks;

    // ── helpers ───────────────────────────────────────────────────────────────

    bool shouldSkip(clang::SourceLocation Loc) const {
        if (Loc.isInvalid()) return true;
        return SM.isInSystemHeader(SM.getSpellingLoc(Loc));
    }

    std::string fileOf(clang::SourceLocation Loc) const {
        auto FERef = SM.getFileEntryRefForID(
            SM.getFileID(SM.getSpellingLoc(Loc)));
        if (!FERef) return "<unknown>";
        return std::string(FERef->getName());
    }

    unsigned lineOf(clang::SourceLocation Loc) const {
        return SM.getSpellingLineNumber(Loc);
    }

    std::string extractRangeText(clang::SourceRange Range) const {
        clang::SourceLocation B = SM.getSpellingLoc(Range.getBegin());
        clang::SourceLocation E = SM.getSpellingLoc(Range.getEnd());
        if (B.isInvalid() || E.isInvalid()) return "";
        bool Invalid = false;
        const char *Start = SM.getCharacterData(B, &Invalid);
        if (Invalid || !Start) return "";
        const char *End   = SM.getCharacterData(E, &Invalid);
        if (Invalid || !End || End < Start) return "";
        std::string text(Start, static_cast<size_t>(End - Start + 1));
        auto first = text.find_first_not_of(" \t\r\n");
        auto last  = text.find_last_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        return text.substr(first, last - first + 1);
    }

    void pushSentinel() {
        StackFrame f;
        f.sentinel = true;
        stack.push(std::move(f));
    }

    void pushBlock(clang::SourceLocation Loc,
                   const std::string &kind,
                   const std::string &cond) {
        Block blk;
        blk.file          = fileOf(Loc);
        blk.top_kind      = kind;
        blk.top_condition = cond;
        blk.start_line    = lineOf(Loc);

        Branch br;
        br.kind       = kind;
        br.condition  = cond;
        br.start_line = lineOf(Loc);

        stack.push({std::move(blk), std::move(br)});
    }

    void openBranch(clang::SourceLocation Loc,
                    const std::string &kind,
                    const std::string &cond) {
        if (stack.empty()) return;
        Branch br;
        br.kind       = kind;
        br.condition  = cond;
        br.start_line = lineOf(Loc);
        stack.top().currentBranch = std::move(br);
    }

    void closeTopBranch(clang::SourceLocation Loc) {
        if (stack.empty()) return;
        auto &frame = stack.top();
        unsigned endLine = lineOf(Loc);
        if (endLine > 0) --endLine;
        frame.currentBranch.end_line = endLine;
        if (!frame.blk.file.empty())
            frame.blk.branches.push_back(frame.currentBranch);
    }

    void popBlock(clang::SourceLocation Loc) {
        if (stack.empty()) return;
        auto frame = std::move(stack.top());
        stack.pop();
        if (frame.blk.file.empty()) return;  // was sentinel
        frame.blk.end_line = lineOf(Loc);
        fileBlocks[frame.blk.file].push_back(std::move(frame.blk));
    }
};

// ─── ASTConsumer that triggers the JSON flush ─────────────────────────────────
// HandleTranslationUnit is guaranteed to be called after all PPCallbacks fire.

class IfdefMapperConsumer : public clang::ASTConsumer {
public:
    IfdefMapperConsumer(IfdefMapperCallbacks *cb, std::string outPath)
        : cb(cb), outPath(std::move(outPath)) {}

    void HandleTranslationUnit(clang::ASTContext &) override {
        if (cb) cb->flush(outPath);
    }

private:
    IfdefMapperCallbacks *cb;       // non-owning; valid for full TU lifetime
    std::string           outPath;
};

// ─── Plugin action ────────────────────────────────────────────────────────────

class IfdefMapperAction : public clang::PluginASTAction {
public:
    std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(clang::CompilerInstance &CI,
                      llvm::StringRef) override {
        auto cb = std::make_unique<IfdefMapperCallbacks>(CI.getSourceManager());
        auto *rawCb = cb.get();
        CI.getPreprocessor().addPPCallbacks(std::move(cb));
        return std::make_unique<IfdefMapperConsumer>(rawCb, outPath);
    }

    bool ParseArgs(const clang::CompilerInstance &,
                   const std::vector<std::string> &args) override {
        for (const auto &arg : args) {
            if (arg.rfind("output=", 0) == 0)
                outPath = arg.substr(7);
        }
        return true;
    }

    ActionType getActionType() override {
        return CmdlineBeforeMainAction;
    }

private:
    std::string outPath = "ast_mapping.json";
};

} // anonymous namespace

static clang::FrontendPluginRegistry::Add<IfdefMapperAction>
    X("ifdef-mapper", "Record #ifdef/#ifndef/#if blocks with source locations");
