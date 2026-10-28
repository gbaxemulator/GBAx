#define COMMON_H
#define PNG_H
#define OPAQUE_THREADING
#define _SYS_TIME_H
#define _SYS_TIME_H_
#define _TIME_H
#define _TIME_H_
#define GBAX_EXPORT

#define ATTRIBUTE_FORMAT(X, Y, Z)
#define ATTRIBUTE_NONSTRING
#define DECL_BITFIELD(newtype, oldtype) typedef oldtype newtype
#define DECL_BIT(type, field, bit) DECL_BITS(type, field, bit, 1)
#define DECL_BITS(TYPE, FIELD, START, SIZE) \
	TYPE TYPE ## Is ## FIELD (TYPE); \
	TYPE TYPE ## Get ## FIELD (TYPE); \
	TYPE TYPE ## Clear ## FIELD (TYPE); \
	TYPE TYPE ## Fill ## FIELD (TYPE); \
	TYPE TYPE ## Set ## FIELD (TYPE, TYPE); \
	TYPE TYPE ## TestFill ## FIELD (TYPE, bool);

#define CXX_GUARD_START
#define CXX_GUARD_END

#define PYCPARSE
typedef ... va_list;
typedef int... time_t;
typedef int... off_t;
typedef ...* png_structp;
typedef ...* png_infop;
typedef ...* png_unknown_chunkp;

void free(void*);

#include <limits.h>

#include <gbax/flags.h>

#include <gbax/core/cache-set.h>
#include <gbax/core/core.h>
#include <gbax/core/map-cache.h>
#include <gbax/core/mem-search.h>
#include <gbax/core/thread.h>
#include <gbax/core/version.h>

#define PYEXPORT extern "Python+C"
#include "platform/python/core.h"
#include "platform/python/log.h"
#include "platform/python/vfs-py.h"
#undef PYEXPORT

#ifdef USE_PNG
#include <gbax-util/image/png-io.h>
#endif
#ifdef M_CORE_GBA
#include <gbax/gba/interface.h>
#include <gbax/internal/arm/arm.h>
#include <gbax/internal/gba/gba.h>
#include <gbax/internal/gba/input.h>
#include <gbax/internal/gba/renderers/cache-set.h>
#endif
#ifdef M_CORE_GB
#include <gbax/internal/sm83/sm83.h>
#include <gbax/internal/gb/gb.h>
#include <gbax/internal/gba/input.h>
#include <gbax/internal/gb/renderers/cache-set.h>
#endif
#ifdef ENABLE_DEBUGGERS
#include <gbax/debugger/debugger.h>
#include <gbax/internal/debugger/cli-debugger.h>
#endif
