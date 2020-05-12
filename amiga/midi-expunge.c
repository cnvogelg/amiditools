#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/camd.h>

#include <midi/camd.h>

static const char *TEMPLATE = 
   "NAME/A";
typedef struct {
  char *dev_name;
} params_t;
static params_t params;

struct Library *CamdBase;
struct DosLibrary *DOSBase;

int main(int argc, char **argv)
{
    struct RDArgs *args;

    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 0L);

    /* First parse args */
    args = ReadArgs(TEMPLATE, (LONG *)&params, NULL);
    if(args == NULL) {
        PrintFault(IoErr(), "Args Error");
        return RETURN_ERROR;
    }

    CamdBase = OpenLibrary((UBYTE *)"camd.library", 0L);
    if(CamdBase != NULL) {

        char *name = params.dev_name;
        Printf("Opening midi device: %s\n", name);
        struct MidiDeviceData *data = OpenMidiDevice(name);
        if(data != NULL) {
            Printf("Closing midi device: %s\n", name);
            CloseMidiDevice(data);
            PutStr("Done.\n");
        } else {
            PutStr("Failed!\n");
        }

        CloseLibrary(CamdBase);
    } else {
        PutStr("Error opening 'camd.library'!\n");
    }

    FreeArgs(args);

    CloseLibrary((struct Library *)DOSBase);
    return 0;
}