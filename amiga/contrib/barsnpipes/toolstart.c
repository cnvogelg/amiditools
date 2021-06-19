
    /*                                                        *
    * The startfile,                                          *
    * that (compiled as toolstart.o) always MUST linked       *
    * BEFORE the mytool.o.                                    *
    *                                                         *
    * example for DICE:                                       *
    *                                                         *
    * dlink toolstart.o mytools.o amigas.lib -o mytool.ptool  *
    *       ^^^^^^^^^^^                                       */


//#include "compilerspecific.h"
#include <exec/types.h>
#include "v.h"

#define SAVEDS __saveds

long IntuitionBase;
long GfxBase;
long LayersBase;
long DOSBase;
long SysBase;
long stdout;
struct Functions *functions;
extern struct ToolMaster *inittoolmaster(void);


SAVEDS struct ToolMaster *start(struct Functions *f)
{
    functions = f;
    if (f->version < 3) return(0);      /* Make sure this is B&P Pro! */
    SysBase = functions->SysBase;
    DOSBase = functions->DOSBase;
    IntuitionBase = functions->IntuitionBase;
    GfxBase = functions->GfxBase;
    LayersBase = functions->LayersBase;
    return((struct ToolMaster *)inittoolmaster());
}


