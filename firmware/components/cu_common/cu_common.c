#include "cu_common.h"

const char *cu_quality_name(cu_quality_t q)
{
    switch (q) {
    case CU_Q_OK: return "ok";
    case CU_Q_STALE: return "stale";
    case CU_Q_OPEN: return "open";
    case CU_Q_SHORT: return "short";
    case CU_Q_IMPLAUSIBLE: return "implausible";
    case CU_Q_UNCALIBRATED: return "uncalibrated";
    }
    return "?";
}
