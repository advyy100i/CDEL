// DeadFeaturePass.cpp — LLVM new-pass-manager Module pass.
//
// Performs whole-program call-graph reachability analysis starting from main
// and all exported (external-linkage) definitions, then correlates reachable
// basic blocks with debug-info line numbers.
//
// Output: reachability.json — used by correlator.py (Phase 4) to determine
//         which #ifdef-guarded line ranges are never covered.
//
// Build:  bash build.sh
// Invoke:
//   opt --load-pass-plugin=./DeadFeaturePass.so \
//       --passes="dead-feature-pass" \
//       --dead-feature-output=reachability.json \
//       whole_program.bc --disable-output

#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Value.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Plugins/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

// ── command-line option for output path ──────────────────────────────────────
static llvm::cl::opt<std::string> OutputFile(
    "dead-feature-output",
    llvm::cl::desc("Output JSON file for DeadFeaturePass reachability data"),
    llvm::cl::value_desc("filename"),
    llvm::cl::init("reachability.json"));

namespace {

// ── JSON helpers (stdlib-only) ────────────────────────────────────────────────

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
        default:   out += c;
        }
    }
    return out;
}

// ── File path resolution ──────────────────────────────────────────────────────

// Combine directory + filename into an absolute path (best-effort).
static std::string resolveFilePath(llvm::StringRef dir, llvm::StringRef file) {
    if (file.empty()) return "<unknown>";
    // If file is already absolute, use it directly.
    if (!file.empty() && file[0] == '/') return file.str();
    if (dir.empty()) return file.str();
    return (dir + "/" + file).str();
}

static std::string fileFromDILocation(const llvm::DILocation *Loc) {
    if (!Loc) return "<unknown>";
    const llvm::DIFile *F = Loc->getFile();
    if (!F) return "<unknown>";
    return resolveFilePath(F->getDirectory(), F->getFilename());
}

// ── Reachability data structures ──────────────────────────────────────────────

struct BlockInfo {
    std::string              name;
    std::vector<unsigned>    lines;
};

struct FunctionInfo {
    std::string              mangledName;
    std::string              sourceFile;
    std::vector<BlockInfo>   blocks;
    std::vector<unsigned>    allLines;  // union of lines across all blocks
};

// ── BFS call-graph reachability ───────────────────────────────────────────────

static std::set<llvm::Function *> computeReachable(llvm::Module &M) {
    std::set<llvm::Function *>   reachable;
    std::queue<llvm::Function *> worklist;

    auto enqueue = [&](llvm::Function *F) {
        if (F && !F->isDeclaration() && reachable.insert(F).second)
            worklist.push(F);
    };

    // Seed: main + any external-linkage definition (exported symbols).
    for (auto &F : M) {
        if (F.isDeclaration()) continue;
        if (F.getName() == "main" || F.hasExternalLinkage())
            enqueue(&F);
    }

    // BFS over direct callees.
    while (!worklist.empty()) {
        auto *F = worklist.front();
        worklist.pop();

        for (auto &BB : *F) {
            for (auto &I : BB) {
                if (auto *CB = llvm::dyn_cast<llvm::CallBase>(&I)) {
                    enqueue(CB->getCalledFunction());  // null-safe via enqueue
                }
            }
        }
    }

    // Conservative indirect-call handling: if any reachable function makes an
    // indirect call, include all address-taken non-declaration functions.
    bool hasIndirectCall = false;
    for (auto *F : reachable) {
        for (auto &BB : *F) {
            for (auto &I : BB) {
                if (auto *CB = llvm::dyn_cast<llvm::CallBase>(&I)) {
                    if (!CB->getCalledFunction()) {
                        hasIndirectCall = true;
                        goto done_scan;
                    }
                }
            }
        }
    }
done_scan:
    if (hasIndirectCall) {
        for (auto &F : M) {
            if (!F.isDeclaration() && F.hasAddressTaken())
                enqueue(&F);
        }
        // Re-run BFS for newly added address-taken functions.
        while (!worklist.empty()) {
            auto *F = worklist.front();
            worklist.pop();
            for (auto &BB : *F)
                for (auto &I : BB)
                    if (auto *CB = llvm::dyn_cast<llvm::CallBase>(&I))
                        enqueue(CB->getCalledFunction());
        }
    }

    return reachable;
}

// ── Line/block info extraction ────────────────────────────────────────────────

static FunctionInfo extractFunctionInfo(llvm::Function &F) {
    FunctionInfo fi;
    fi.mangledName = F.getName().str();

    // Try to get source file from function's own debug info subprogram.
    if (auto *SP = F.getSubprogram()) {
        fi.sourceFile = resolveFilePath(SP->getDirectory(), SP->getFilename());
    }

    std::set<unsigned> allLinesSet;

    for (auto &BB : F) {
        BlockInfo bi;
        bi.name = BB.hasName() ? BB.getName().str() : "<unnamed>";
        std::set<unsigned> blockLines;

        for (auto &I : BB) {
            const llvm::DebugLoc &DL = I.getDebugLoc();
            if (!DL) continue;
            unsigned line = DL.getLine();
            if (line == 0) continue;

            // Prefer the file from the first debug location if not yet set.
            if (fi.sourceFile.empty() || fi.sourceFile == "<unknown>") {
                fi.sourceFile = fileFromDILocation(DL.get());
            }

            blockLines.insert(line);
            allLinesSet.insert(line);
        }

        bi.lines.assign(blockLines.begin(), blockLines.end());
        fi.blocks.push_back(std::move(bi));
    }

    fi.allLines.assign(allLinesSet.begin(), allLinesSet.end());
    return fi;
}

// ── JSON writer ───────────────────────────────────────────────────────────────

static void writeJSON(const std::string &outPath,
                      const std::vector<std::string> &entryPoints,
                      const std::vector<FunctionInfo> &reachable,
                      const std::vector<std::string> &unreachable,
                      const std::map<std::string, std::set<unsigned>> &reachableLines)
{
    std::ofstream f(outPath);
    if (!f) {
        llvm::errs() << "[DeadFeaturePass] ERROR: cannot open output: " << outPath << "\n";
        return;
    }

    // ── entry points ──────────────────────────────────────────────────────────
    f << "{\n";
    f << "  \"tool\": \"DeadFeaturePass\",\n";
    f << "  \"entry_points\": [";
    for (size_t i = 0; i < entryPoints.size(); ++i) {
        if (i) f << ", ";
        f << "\"" << jsonEscape(entryPoints[i]) << "\"";
    }
    f << "],\n";

    // ── reachable function names ──────────────────────────────────────────────
    f << "  \"reachable_functions\": [";
    for (size_t i = 0; i < reachable.size(); ++i) {
        if (i) f << ", ";
        f << "\"" << jsonEscape(reachable[i].mangledName) << "\"";
    }
    f << "],\n";

    // ── unreachable function names ────────────────────────────────────────────
    f << "  \"unreachable_functions\": [";
    for (size_t i = 0; i < unreachable.size(); ++i) {
        if (i) f << ", ";
        f << "\"" << jsonEscape(unreachable[i]) << "\"";
    }
    f << "],\n";

    // ── per-file reachable line sets ──────────────────────────────────────────
    f << "  \"reachable_lines\": {\n";
    bool firstFile = true;
    for (const auto &[file, lines] : reachableLines) {
        if (!firstFile) f << ",\n";
        firstFile = false;
        f << "    \"" << jsonEscape(file) << "\": [";
        bool first = true;
        for (unsigned ln : lines) {
            if (!first) f << ", ";
            first = false;
            f << ln;
        }
        f << "]";
    }
    f << "\n  },\n";

    // ── per-function detail (blocks + lines) ──────────────────────────────────
    f << "  \"function_details\": {\n";
    bool firstFn = true;
    for (const auto &fi : reachable) {
        if (!firstFn) f << ",\n";
        firstFn = false;
        f << "    \"" << jsonEscape(fi.mangledName) << "\": {\n";
        f << "      \"source_file\": \"" << jsonEscape(fi.sourceFile) << "\",\n";
        f << "      \"lines\": [";
        for (size_t i = 0; i < fi.allLines.size(); ++i) {
            if (i) f << ", ";
            f << fi.allLines[i];
        }
        f << "],\n";
        f << "      \"basic_blocks\": [\n";
        for (size_t bi = 0; bi < fi.blocks.size(); ++bi) {
            const auto &blk = fi.blocks[bi];
            if (bi) f << ",\n";
            f << "        { \"name\": \"" << jsonEscape(blk.name) << "\","
              << " \"lines\": [";
            for (size_t li = 0; li < blk.lines.size(); ++li) {
                if (li) f << ", ";
                f << blk.lines[li];
            }
            f << "] }";
        }
        f << "\n      ]\n";
        f << "    }";
    }
    f << "\n  }\n}\n";
    f.flush();

    llvm::errs() << "[DeadFeaturePass] Wrote reachability data for "
                 << reachable.size() << " function(s) → " << outPath << "\n";
}

// ── The pass itself ───────────────────────────────────────────────────────────

struct DeadFeaturePass : llvm::PassInfoMixin<DeadFeaturePass> {

    llvm::PreservedAnalyses run(llvm::Module &M,
                                llvm::ModuleAnalysisManager & /*MAM*/) {
        const std::string outPath = OutputFile.getValue();

        // ── compute reachable set ─────────────────────────────────────────────
        auto reachableSet = computeReachable(M);

        std::vector<std::string> entryPoints;
        std::vector<FunctionInfo> reachableInfos;
        std::vector<std::string> unreachableNames;
        std::map<std::string, std::set<unsigned>> reachableLines;

        for (auto &F : M) {
            if (F.isDeclaration()) continue;
            if (reachableSet.count(&F)) {
                FunctionInfo fi = extractFunctionInfo(F);
                // Collect into per-file line map.
                if (!fi.sourceFile.empty() && fi.sourceFile != "<unknown>") {
                    for (unsigned ln : fi.allLines)
                        reachableLines[fi.sourceFile].insert(ln);
                }
                reachableInfos.push_back(std::move(fi));
            } else {
                unreachableNames.push_back(F.getName().str());
            }
        }

        // Determine entry points (already found in computeReachable seeds).
        for (auto &F : M) {
            if (F.isDeclaration()) continue;
            if (F.getName() == "main" || F.hasExternalLinkage())
                entryPoints.push_back(F.getName().str());
        }

        writeJSON(outPath, entryPoints, reachableInfos, unreachableNames,
                  reachableLines);

        return llvm::PreservedAnalyses::all();
    }

    static bool isRequired() { return true; }
};

} // anonymous namespace

// ── Plugin registration (new pass manager) ────────────────────────────────────

llvm::PassPluginLibraryInfo getDeadFeaturePassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION,
        "DeadFeaturePass",
        LLVM_VERSION_STRING,
        [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef Name,
                   llvm::ModulePassManager &MPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) -> bool {
                    if (Name == "dead-feature-pass") {
                        MPM.addPass(DeadFeaturePass());
                        return true;
                    }
                    return false;
                });
        }
    };
}

extern "C" LLVM_ATTRIBUTE_WEAK llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return getDeadFeaturePassPluginInfo();
}
