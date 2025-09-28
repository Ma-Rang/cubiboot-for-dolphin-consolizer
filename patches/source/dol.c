#include <gctypes.h>

#include "dol.h"

u32 DOLMax(DOLHEADER * dol) {
    int i;
    u32 maxaddress = 0;

    /*** Go through DOL sections ***/
    /*** Text sections ***/
    for (i = 0; i < MAXTEXTSECTION; i++)
    {
        if (dol->textAddress[i] && dol->textLength[i])
        {
            if ((dol->textAddress[i] + dol->textLength[i]) > maxaddress) 
                maxaddress = dol->textAddress[i] + dol->textLength[i];
        }
    }

    /*** Data sections ***/
    for (i = 0; i < MAXDATASECTION; i++)
    {
        if (dol->dataAddress[i] && dol->dataLength[i])
        {
            if ((dol->dataAddress[i] + dol->dataLength[i]) > maxaddress)
                maxaddress = dol->dataAddress[i] + dol->dataLength[i];
        }
    }

    /*** And of course, any BSS section ***/
    if (dol->bssAddress)
    {
        if ((dol->bssAddress + dol->bssLength) > maxaddress)
            maxaddress = dol->bssAddress + dol->bssLength;
    }

    return maxaddress;
}
