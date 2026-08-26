#include "Epub/Epub/parsers/TableElementPolicy.h"

using EpubTableElementPolicy::isCaption;
using EpubTableElementPolicy::isStandaloneCaption;
using EpubTableElementPolicy::isTransparentCellWrapper;

static_assert(isCaption("caption"));
static_assert(!isCaption("p"));

static_assert(isStandaloneCaption("caption", 1));
static_assert(!isStandaloneCaption("caption", 0));
static_assert(!isStandaloneCaption("caption", 2));
static_assert(!isStandaloneCaption("p", 1));

static_assert(!isTransparentCellWrapper("caption", 1, true));
static_assert(isTransparentCellWrapper("p", 1, true));
static_assert(!isTransparentCellWrapper("p", 0, true));
static_assert(!isTransparentCellWrapper("span", 1, false));

int main() { return 0; }
