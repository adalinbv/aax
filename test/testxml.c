/* -*- mode: C; tab-width:8; c-basic-offset:8 -*-
 * vi:set ts=8:
 *
 * This file is in the Public Domain and comes with no warranty.
 * Erik Hofman <erik@ehofman.com>
 *
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include <xml.h>

#include "driver.h"

#define	FILE_PATH	SRC_PATH"/../docs/openal_sample.xml"

int main(int argc, char **argv)
{
    char *infile = getInputFile(argc, argv, FILE_PATH);
    void *xid;
    
    xid = xmlOpen(infile);
    if (xid)
    {
        float f = 0.0f;
        void *xrid, *xsid;
        char *s = 0;
        int i = 0;

        xrid = xmlMarkId(xid);
        xmlNodeGetPos(xid, xrid, "configuration", 0);

        xsid = xmlNodeGet(xrid, "output");
        if (xsid)
        {
            char str[1024];
            void *sxid;
            int q, qmax;

            i = xmlNodeCopyString(xsid, "device", (char *)&str, 1024);
            if (i) printf("device: %s\n", str);
            else printf("Error: '/configuration/output/device' not found.\n");

            qmax = xmlNodeGetNum(xsid, "speaker");
            printf("no. speakers declared: %i\n", qmax);

            sxid = xmlMarkId(xsid);
            for(q=0; q<qmax; q++)
            {
               char attribute[64];
               float v[3];

               xmlNodeGetPos(xsid, sxid, "speaker", q);
               
               if (xmlAttributeCopyString(sxid, "n", (char *)&attribute, 64))
                  printf("speaker #%i, n=\"%s\"\n", q, attribute);
               else
                  printf("speaker #%i, attribute 'n' is not defined\n", q);

               v[0] = xmlNodeGetDouble(sxid, "pos-x");
               v[1] = xmlNodeGetDouble(sxid, "pos-y");
               v[2] = xmlNodeGetDouble(sxid, "pos-z");
               printf("speaker #%i pos-x: %5.2f, pos-y: %5.2f, pos-z: %5.2f\n",
                       q, v[0], v[1], v[2]);
            }
            free(xsid);
            free(sxid);
        }
        else printf("Error: '/configuration/output' not found.\n");
        free(xrid);
        
        f = xmlNodeGetDouble(xid, "/configuration/backend/output/frequency-hz");
        printf("output frequency: %8.1f Hz\n", f);

        s = xmlNodeGetString(xid, "/configuration/output/setup");
        if (s)
        {
           printf("output type: '%s'\n", s);
           free(s);
        }
        else
           printf("Error: '/configuration/output/setup' not found.\n");

        s = xmlNodeGetString(xid, "/configuration/Null/device");
        if (s)
        {
           printf("\nERROR: Null device: %s\n", s);
           free(s);
        }
        else printf("No device found in the Null section, this is ok.\n");

        xmlClose(xid);
    }
    else
        printf("Input file not found: %s\n", infile);

    printf("\n");

    return 0;
}
