/* quince/libc/unistd.h: QUINCE's own (M1). console.c includes it: its
 * -condebug log is left out on the board (quake/console.c, QUINCE:), and
 * Con_Init's unlink is the SDK's, through quince/qsys.h. */
int qsys_unlink(const char *path);
#define unlink qsys_unlink
