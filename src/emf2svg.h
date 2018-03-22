#include <stdbool.h>
#include <stddef.h> /* for offsetof() macro */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// structure containing generator arguments
typedef struct {
    // SVG namespace (the '<something>:' before each fields)
    char *nameSpace;
    // Verbose mode, output fields and fields values if True
    bool verbose;
    // Handle emf+ records or not
    bool emfplus;
    // draw svg document delimiter or not
    bool svgDelimiter;
    // height of the target image
    double imgHeight;
    // width of the target image
    double imgWidth;
    // Embedded Images (and in the case of HTML, svg pages) are stored on disk
    bool linkResources;
    // Path for resources
    const char *resourcePath;
    //------------ HTML print page options
    // HTML page width
    unsigned pageWidth;
    // HTML page height
	unsigned pageHeight;
    // HTML - reserved orientation
    bool     pageLandscape;
    // HTML - left margin
    unsigned leftMargin;
    // HTML - right margin
    unsigned rightMargin;
    // HTML - lower margin
    unsigned lowerMargin;
    // HTML - upper margin
    unsigned upperMargin;
} generatorOptions;

// convert functions
int emf2svg(char *contents, size_t length, char **out, size_t *out_length,
            generatorOptions *options);
int femf2svg(const char *infilename, const char * outputfilename, 
             generatorOptions *options);
int femf2html(int nfilename,const char **infilename, const char * outputfilename, 
             generatorOptions *options);

// check if emf file contains emf+ records
int emf2svg_is_emfplus(char *contents, size_t length, bool *is_emfp);

// scan a list of directories to build a font index (index[<FONT_NAME>] =
// <FONT_PATH>)
int emf2svg_gen_font_index(char **font_paths, void *font_index);

// free the font index
int emf2svg_free_font_index(void *font_index);
//! \endcond

#ifdef __cplusplus
}
#endif

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
