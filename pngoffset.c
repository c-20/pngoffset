#include <stddef.h>
#include "code.h"

// #include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>

#define PNG_DEBUG 3
#include <png.h>

typedef struct _IMAGE {
  png_bytepp rowdata;
  IN width, height;
  png_byte colourtype;
  png_byte bitdepth;  
  struct _IMAGE *next;
} IMAGE;

IMAGE pngopenfile(CS filename) {
  IMAGE image;
  image.rowdata = NULL;
  image.width = 0;
  image.height = 0;
  image.next = NULL;
  FS file = OPENBINARYFILE(filename);
  IF (!file)
    { printf("pngreadfile: failed to open file\n"); RT image; }
  CH head[8];
  fread(head, 1, 8, file);
  IF (png_sig_cmp(head, 0, 8))
    { printf("pngreadfile: png signature mismatch\n"); CLOSEFILE(file); RT image; }
  png_structp pngptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  IF (!pngptr)
    { printf("pngreadfile: failed to create read struct\n"); CLOSEFILE(file); RT image; }
  png_infop pnginfoptr = png_create_info_struct(pngptr);
  IF (!pnginfoptr)
    { printf("pngreadfile: failed to create info struct\n"); CLOSEFILE(file); RT image; }
  IF (setjmp(png_jmpbuf(pngptr)))
    { printf("pngreadfile: png jumpbuf readinfo setjmp error\n"); CLOSEFILE(file); RT image; }
  png_init_io(pngptr, file);
  png_set_sig_bytes(pngptr, 8);
  png_read_info(pngptr, pnginfoptr);
  image.width = png_get_image_width(pngptr, pnginfoptr);
  image.height = png_get_image_height(pngptr, pnginfoptr);
  image.colourtype = png_get_color_type(pngptr, pnginfoptr);
  image.bitdepth = png_get_bit_depth(pngptr, pnginfoptr);
  IN numberofpasses = png_set_interlace_handling(pngptr);
  png_read_update_info(pngptr, pnginfoptr);
  IF (setjmp(png_jmpbuf(pngptr)))
    { printf("pngreadfile: png jumpbuf readdata setjmp error\n"); CLOSEFILE(file); RT image; }
  image.rowdata = COUNTMEM(png_bytep, image.height);
  IN y = -1;
  WI (INC y LT image.height)
    { image.rowdata[y] = COUNTMEM(png_byte, png_get_rowbytes(pngptr, pnginfoptr)); }
  png_read_image(pngptr, image.rowdata);
  png_destroy_read_struct(&pngptr, &pnginfoptr, NULL);
  CLOSEFILE(file);
  RT image;
}

CH pngwritefile(IMAGE *image, CS filename) { // consider lists...
  FS file = SAVEBINARYFILE(filename);
  IF (!file)
    { printf("pngwritefile: failed to open file for writing\n"); RT 0; }
  png_structp pngptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  IF (!pngptr)
    { printf("pngwritefile: failed to create write struct\n"); CLOSEFILE(file); RT 0; }
  png_infop pnginfoptr = png_create_info_struct(pngptr);
  IF (!pnginfoptr)
    { printf("pngwritefile: failed to create info struct\n"); CLOSEFILE(file); RT 0; }
  IF (setjmp(png_jmpbuf(pngptr)))
    { printf("pngwritefile: png jumpbuf writeinit setjmp error\n"); CLOSEFILE(file); RT 0; }
  png_init_io(pngptr, file);
  IF (setjmp(png_jmpbuf(pngptr)))
    { printf("pngwritefile: png jumpbuf writeinfo setjmp error\n"); CLOSEFILE(file); RT 0; }
  png_set_IHDR(pngptr, pnginfoptr, image->width, image->height, image->bitdepth, image->colourtype,
                           PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
  png_write_info(pngptr, pnginfoptr);
  IF (setjmp(png_jmpbuf(pngptr)))
    { printf("pngwritefile: png jumpbuf writedata setjmp error\n"); CLOSEFILE(file); RT 0; }
  png_write_image(pngptr, image->rowdata);
  IF (setjmp(png_jmpbuf(pngptr)))
    { printf("pngwritefile: png jumpbuf writeend setjmp error\n"); CLOSEFILE(file); RT 0; }
  png_write_end(pngptr, NULL);
  png_destroy_write_struct(&pngptr, &pnginfoptr);
  CLOSEFILE(file);
  RT 1;
}

CH pngcloseimage(IMAGE *image) {
  IN y = -1;
  WI (INC y LT image->height)
    { FREEMEM(image->rowdata[y]); }
  FREEMEM(image->rowdata);
  image->rowdata = NULL;
  RT 1;
} // does not free the pointed IMAGE structure - might be part of a list

// TODO: create series of routines that return a processed IMAGE
CH darkenimage(IMAGE *image, IN offset) {
  CH bytesperpixel = 1; // assume monochrome by default (lol)
  IF (image->colourtype EQ PNG_COLOR_TYPE_RGB)    { bytesperpixel = 3; }
  IF (image->colourtype EQ PNG_COLOR_TYPE_RGBA) { bytesperpixel = 4; }
  IN y = -1;
  WI (INC y LT image->height) {
    png_byte *row = image->rowdata[y];
    IN x = -1;
    WI (INC x LT image->width) {
      png_byte *px = &(row[x * bytesperpixel]);
      IN r = (IN)px[0];
      IN g = (IN)px[1];
      IN b = (IN)px[2];
      IN a = (IN)px[3];
      r SUBS offset;
      g SUBS offset;
      b SUBS offset;
      IF (offset GQ 0) { // subtract has lower bound
        IF (r LT 0) { r = 0; }
        IF (g LT 0) { g = 0; }
        IF (b LT 0) { b = 0; }
      } EL { // negative subtract has upper bound
        IF (r GT 255) { r = 255; }
        IF (g GT 255) { g = 255; }
        IF (b GT 255) { b = 255; }
      }
      px[0] = (png_byte)r;
      px[1] = (png_byte)g;
      px[2] = (png_byte)b;
    }
  }
  RT 1;
}

CH brightenimage(IMAGE *image, IN offset) {
  RT darkenimage(image, 0 - offset);
}

#define pngoffset main
IN pngoffset($) {
  CS filename = ($N GT 0) ? $1 : "image.png";
  CS darkerfilename = "darkerimage.png";
  CS brighterfilename = "brighterimage.png";
  IMAGE image = pngopenfile(filename);
  darkenimage(&image, 100);
  IF (!pngwritefile(&image, darkerfilename))
    { printf("darkerwritefailed\n"); }
  pngcloseimage(&image);
  image = pngopenfile(filename);
  brightenimage(&image, 100);
  IF (!pngwritefile(&image, brighterfilename))
    { printf("brighterwritefailed\n"); }
  pngcloseimage(&image);
  RT 0;
}

