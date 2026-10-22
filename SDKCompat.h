#pragma once
#include "AE_Effect.h"

// SDKs predating the display-text aliases expose plain char field names.
#ifndef PF_DEF_NAME
#define PF_DEF_NAME name
#endif
#ifndef PF_DEF_NAMESPTR
#define PF_DEF_NAMESPTR namesptr
#endif
#ifndef PF_DEF_NAMEPTR
#define PF_DEF_NAMEPTR nameptr
#endif
