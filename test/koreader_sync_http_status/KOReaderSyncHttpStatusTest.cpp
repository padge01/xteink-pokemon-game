#include "KOReaderSync/KOReaderSyncHttpStatus.h"

static_assert(!koreader_sync::isSuccessfulHttpCode(199));
static_assert(koreader_sync::isSuccessfulHttpCode(200));
static_assert(koreader_sync::isSuccessfulHttpCode(204));
static_assert(koreader_sync::isSuccessfulHttpCode(299));
static_assert(!koreader_sync::isSuccessfulHttpCode(300));

static_assert(!koreader_sync::isNoContentProgressCode(200));
static_assert(koreader_sync::isNoContentProgressCode(204));
static_assert(!koreader_sync::isNoContentProgressCode(205));
static_assert(!koreader_sync::isNoContentProgressCode(404));

int main() { return 0; }
