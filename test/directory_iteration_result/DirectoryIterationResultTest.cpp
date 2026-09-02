#include "lib/FsHelpers/DirectoryIterationResult.h"

using FsHelpers::classifyDirectoryIterationEnd;
using FsHelpers::directoryIterationFailed;
using FsHelpers::DirectoryIterationResult;

static_assert(classifyDirectoryIterationEnd(false, false) == DirectoryIterationResult::Complete,
              "Clean EOF must remain distinguishable from iteration failures");
static_assert(classifyDirectoryIterationEnd(true, false) == DirectoryIterationResult::AllocationFailed,
              "Wrapper allocation failure must retain the existing memory-error path");
static_assert(classifyDirectoryIterationEnd(false, true) == DirectoryIterationResult::ReadFailed,
              "An SdFat read error must not be reported as clean EOF");
static_assert(classifyDirectoryIterationEnd(true, true) == DirectoryIterationResult::AllocationFailed,
              "Allocation failure takes precedence when both failure flags are present");
static_assert(!directoryIterationFailed(DirectoryIterationResult::Complete));
static_assert(directoryIterationFailed(DirectoryIterationResult::AllocationFailed));
static_assert(directoryIterationFailed(DirectoryIterationResult::ReadFailed));

int main() { return 0; }
