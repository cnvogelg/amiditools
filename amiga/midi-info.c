#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/camd.h>

#include <midi/camd.h>

struct Library *CamdBase;
struct DosLibrary *DOSBase;

int main(int argc, char **argv)
{
    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 0L);
    CamdBase = OpenLibrary((UBYTE *)"camd.library", 0L);
    if(CamdBase != NULL) {
        struct MidiCluster *cluster;
        struct MidiLink *link;
        struct MidiNode *node;
        APTR lock;

        lock = LockCAMD(CD_Linkages);
        if(lock != NULL) {

            cluster = NextCluster(NULL);
            while(cluster != NULL) {

                Printf("Cluster: %s\n", cluster->mcl_Node.ln_Name);
                
                link = NextClusterLink(cluster, NULL, MLTYPE_Receiver);
                while(link != NULL) {
                    Printf("  Rvc: %s (%s)\n",
                        link->ml_MidiNode->mi_Node.ln_Name,
                        link->ml_ClusterComment);

                    link = NextClusterLink(cluster, link, MLTYPE_Receiver);
                }

                link = NextClusterLink(cluster, NULL, MLTYPE_Sender);
                while(link != NULL) {
                    Printf("  Snd: %s (%s)\n",
                        link->ml_MidiNode->mi_Node.ln_Name,
                        link->ml_ClusterComment);

                    link = NextClusterLink(cluster, link, MLTYPE_Sender);
                }

                cluster = NextCluster(cluster);
            }

            node = NextMidi(NULL);
            while(node != NULL) {
                Printf("Node: %s\n", node->mi_Node.ln_Name);

                link = NextMidiLink(node, NULL, MLTYPE_Receiver);
                while(link != NULL) {
                    Printf("  Rcv: %s\n", link->ml_ClusterComment);
                    link = NextMidiLink(node, link, MLTYPE_Receiver);
                }

                link = NextMidiLink(node, NULL, MLTYPE_Sender);
                while(link != NULL) {
                    Printf("  Snd: %s\n", link->ml_ClusterComment);
                    link = NextMidiLink(node, link, MLTYPE_Sender);
                }

                node = NextMidi(node);
            }


            UnlockCAMD(lock);
        } else {
            PutStr("Cannot lock CAMD\n");
        }

 
        CloseLibrary(CamdBase);
    } else {
        PutStr("Error opening 'camd.library'!\n");
    }
    CloseLibrary((struct Library *)DOSBase);
    return 0;
}