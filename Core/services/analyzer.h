#ifndef ANALYZER_H_
#define ANALYZER_H_

#include "qpc.h"

/**************************************************************************************************\
* Public macros
\**************************************************************************************************/

#define FREQ_MAX        500000
#define FREQ_MIN        10000
#define FREQ_POINTS_MAX 1024
#define FREQ_POINTS_MIN 2

/**************************************************************************************************\
* Public type definitions
\**************************************************************************************************/

void Analyzer_ctor(void);

/**************************************************************************************************\
* Public memory declarations
\**************************************************************************************************/

extern QActive *const AO_Analyzer; // opaque pointer

/**************************************************************************************************\
* Public prototypes
\**************************************************************************************************/

void Analyzer_Set_Freq_Range(uint32_t freq_start, uint32_t freq_end, uint32_t num_freq_points);
void Analyzer_Begin_Impedance_Sweep();
void Analyzer_Begin_Offset_Calibration();
void Analyzer_Begin_Gain_Calibration();

#endif // ANALYZER_H_
