#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <jpeglib.h>
#include <tiffio.h>
#include "Pixel.h"
#include "Image.h"

Image::
Image(void)
  : pixels(NULL),
  npixels(0),
  width(0),
  height(0)
{}

Image::
Image(const char *filename)
  : pixels(NULL),
  npixels(0),
  width(0),
  height(0)
{
  Read(filename);
}

Image::
Image(int width, int height)
  : pixels(NULL),
  npixels(width * height),
  width(width),
  height(height)
{
  pixels = new Pixel[npixels];
  assert(pixels);
}

Image::
Image(int width, int height, const Pixel *p)
  : pixels(NULL),
  npixels(width * height),
  width(width),
  height(height)
{
  pixels = new Pixel[npixels];
  assert(pixels);

  for (int i = 0; i < npixels; i++) pixels[i] = p[i];
}

Image::
Image(const Image& image)
  : pixels(NULL),
  npixels(image.npixels),
  width(image.width),
  height(image.height)

{
  pixels = new Pixel[npixels];
  assert(pixels);

  for (int i = 0; i < npixels; i++) pixels[i] = image.pixels[i];
}

Image::
~Image(void)
{
  if (pixels) delete[] pixels;
}

Image& Image::
operator=(const Image& image)
{
  if (pixels) { delete[] pixels; pixels = NULL; }

  npixels = image.npixels;
  width   = image.width;
  height  = image.height;

  pixels = new Pixel[npixels];
  assert(pixels);

  for (int i = 0; i < npixels; i++) pixels[i] = image.pixels[i];

  return *this;
}

int Image::
NPixels(void) const
{
  return npixels;
}

int Image::
Width(void) const
{
  return width;
}

int Image::
Height(void) const
{
  return height;
}

Pixel& Image::
GetPixel(int x, int y)
{
  return pixels[x * height + y];
}

Pixel * Image::
Pixels(void)
{
  return pixels;
}

Pixel * Image::
Pixels(int x)
{
  return &pixels[x * height];
}

Pixel * Image::
operator[](int x)
{
  return Pixels(x);
}

const Pixel * Image::
operator[](int x) const
{
  return &pixels[x * height];
}

void Image::
SetPixel(int x, int y, const Pixel& pixel)
{
  pixels[x * height + y] = pixel;
}

void Image::
Subtract(const Image& image)
{
  if ((width != image.width) || (height != image.height)) {
    fprintf(stderr, "Image height and width do not match\n");
  }

  for (int i = 0; i < width; i++) {
    for (int j = 0; j < height; j++) {
      const Pixel& pixel = (*this)[i][j];

      if ((pixel.Red() != pixel.Blue()) || (pixel.Red() != pixel.Green())) {
        fprintf(stderr, "Pixel values not the same across all color channels!\n");
      }

      const Pixel& pixel2 = (image)[i][j];

      if ((pixel2.Red() != pixel2.Blue()) || (pixel2.Red() != pixel2.Green())) {
        fprintf(stderr, "Pixel values not the same across all color channels!\n");
      }

      Pixel pixdiff = pixel - pixel2;
      double value = pixdiff.Luminance();

      GetPixel(i, j).SetGreen(0);

      if (value >= 0) {
        GetPixel(i, j).SetRed(0);
        GetPixel(i, j).SetBlue(value);
      }

      if (value < 0) {
        GetPixel(i, j).SetRed(-1 * value);
        GetPixel(i, j).SetBlue(0);
      }
    }
  }
}

int Image::
Read(const char *filename)
{
  if (pixels) { delete[] pixels; pixels = NULL; }
  npixels = width = height = 0;

  // Default TIF headers (set for non-TIFs too in case of TIF output):
  orientation = ORIENTATION_BOTLEFT;
  planarconfig = PLANARCONFIG_CONTIG;

  char *input_extension;

  if (!(input_extension = (char *)strrchr(filename, '.'))) {
    fprintf(stderr, "Input file has no extension (e.g., .jpg).\n");
    return 0;
  }

  if (strcasecmp(input_extension, ".bmp") == 0) return ReadBMP(filename);
  else if (strcasecmp(input_extension, ".ppm") == 0) return ReadPPM(filename);
  else if (strcasecmp(input_extension, ".jpg") == 0) return ReadJPEG(filename);
  else if (strcasecmp(input_extension, ".jpeg") == 0) return ReadJPEG(filename);
  else if (strcasecmp(input_extension, ".tif") == 0) return ReadTIFF(filename);
  else if (strcasecmp(input_extension, ".tiff") == 0) return ReadTIFF(filename);

  fprintf(stderr, "Unrecognized image file extension");
  return 0;
}

int Image::
Write(const char *filename) const
{
  char *input_extension;

  if (!(input_extension = (char *)strrchr(filename, '.'))) {
    fprintf(stderr, "Input file has no extension (e.g., .jpg).\n");
    return 0;
  }

  if (strcasecmp(input_extension, ".bmp") == 0) return WriteBMP(filename);
  else if (strcasecmp(input_extension, ".ppm") == 0) return WritePPM(filename, 1);
  else if (strcasecmp(input_extension, ".jpg") == 0) return WriteJPEG(filename);
  else if (strcasecmp(input_extension, ".jpeg") == 0) return WriteJPEG(filename);
  else if (strcasecmp(input_extension, ".tif") == 0) return WriteTIFF(filename);
  else if (strcasecmp(input_extension, ".tiff") == 0) return WriteTIFF(filename);

  fprintf(stderr, "Unrecognized image file extension");
  return 0;
}

////////////////////////////////////////////////////////////////////////
// TIFF I/O
////////////////////////////////////////////////////////////////////////

int Image::
ReadTIFF(const char *filename)
{
  // Open file
  TIFF *tif=TIFFOpen(filename, "r");

  if (!tif) {
    fprintf(stderr, "Unable to open image file: %s\n", filename);
    return 0;
  }

  TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
  TIFFGetField(tif, TIFFTAG_ORIENTATION, &orientation);
  TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planarconfig);

  npixels = width * height;

  // Allocate pixels for image
  pixels = new Pixel[npixels];

  if (!pixels) {
    fprintf(stderr, "Unable to allocate memory for TIFF file\n");
    TIFFClose(tif);
    return 0;
  }

  uint32 *raster;
  raster=(uint32 *) _TIFFmalloc(npixels *sizeof(uint32));
  if (!raster) {
    fprintf(stderr, "Unable to allocate temporary memory for TIFF file\n");
    TIFFClose(tif);
    return 0;
  }

  if (TIFFReadRGBAImage(tif, width, height, raster, orientation) != 1) {
    fprintf(stderr, "Unable to read TIFF file\n");
    TIFFClose(tif);
    return 0;
  }

  // Close file
  TIFFClose(tif);

  // Assign pixels
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      double r, g, b, a;

      r = (double)TIFFGetR(raster[j * width + i]) / 255;
      g = (double)TIFFGetG(raster[j * width + i]) / 255;
      b = (double)TIFFGetB(raster[j * width + i]) / 255;
      a = (double)TIFFGetA(raster[j * width + i]) / 255;
      Pixel pixel(r, g, b, a);
      SetPixel(i, j, pixel);
    }
  }

  // Free unsigned char buffer for reading pixels
  _TIFFfree(raster);

  // Return success
  return 1;
}

int Image::
WriteTIFF(const char *filename) const
{
  TIFF *out= TIFFOpen(filename, "w");
  int sampleperpixel = 3; // no alpha channel
  tsize_t linebytes = sampleperpixel * width;

  // Set image metadata
  TIFFSetField(out, TIFFTAG_IMAGEWIDTH, width);
  TIFFSetField(out, TIFFTAG_IMAGELENGTH, height);
  TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, sampleperpixel);
  TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 8);
  TIFFSetField(out, TIFFTAG_ORIENTATION, orientation);
  TIFFSetField(out, TIFFTAG_PLANARCONFIG, planarconfig);
  TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
  TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(out, width * sampleperpixel));

  // allocate scan line buffer
  unsigned char *line = NULL;
  if (TIFFScanlineSize(out) == linebytes)
    line =(unsigned char *)_TIFFmalloc(linebytes);
  else
    line = (unsigned char *)_TIFFmalloc(TIFFScanlineSize(out));

  // Fill buffer with pixels
  for (int j = 0; j < height; j++) {
    unsigned char *p = &line[0];

    for (int i = 0; i < width; i++) {
      const Pixel& pixel = (*this)[i][j];
      int r = (int)(255 * pixel.Red());
      int g = (int)(255 * pixel.Green());
      int b = (int)(255 * pixel.Blue());

      if (r > 255) r = 255;
      if (g > 255) g = 255;
      if (b > 255) b = 255;

      *(p++) = (unsigned char)r;
      *(p++) = (unsigned char)g;
      *(p++) = (unsigned char)b;
    }
    TIFFWriteScanline(out, line, j, 0);
  }

  // Close file
  TIFFClose(out);

  // Free buffer
  _TIFFfree(line);

  // Return number of bytes written
  return 1;
}

////////////////////////////////////////////////////////////////////////
// BMP I/O
////////////////////////////////////////////////////////////////////////

#if (RN_OS == RN_LINUX) && !WIN32

typedef struct tagBITMAPFILEHEADER {
  unsigned short int bfType;
  unsigned int       bfSize;
  unsigned short int bfReserved1;
  unsigned short int bfReserved2;
  unsigned int       bfOffBits;
} BITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER {
  unsigned int       biSize;
  int                biWidth;
  int                biHeight;
  unsigned short int biPlanes;
  unsigned short int biBitCount;
  unsigned int       biCompression;
  unsigned int       biSizeImage;
  int                biXPelsPerMeter;
  int                biYPelsPerMeter;
  unsigned int       biClrUsed;
  unsigned int       biClrImportant;
} BITMAPINFOHEADER;

typedef struct tagRGBTRIPLE {
  unsigned char rgbtBlue;
  unsigned char rgbtGreen;
  unsigned char rgbtRed;
} RGBTRIPLE;

typedef struct tagRGBQUAD {
  unsigned char rgbBlue;
  unsigned char rgbGreen;
  unsigned char rgbRed;
  unsigned char rgbReserved;
} RGBQUAD;

#endif // if (RN_OS == RN_LINUX) && !WIN32

#define BI_RGB        0L
#define BI_RLE8       1L
#define BI_RLE4       2L
#define BI_BITFIELDS  3L

#define BMP_BF_TYPE 0x4D42 /* word BM */
#define BMP_BF_OFF_BITS 54 /* 14 for file header + 40 for info header (not sizeof(), but packed
                              size) */
#define BMP_BI_SIZE 40     /* packed size of info header */


static unsigned short int WordReadLE(FILE *fp)
{
  // Read a unsigned short int from a file in little endian format
  unsigned short int lsb, msb;

  lsb = getc(fp);
  msb = getc(fp);
  return (msb << 8) | lsb;
}

static void WordWriteLE(unsigned short int x, FILE *fp)
{
  // Write a unsigned short int to a file in little endian format
  unsigned char lsb = (unsigned char)(x & 0x00FF); putc(lsb, fp);
  unsigned char msb = (unsigned char)(x >> 8); putc(msb, fp);
}

static unsigned int DWordReadLE(FILE *fp)
{
  // Read a unsigned int word from a file in little endian format
  unsigned int b1 = getc(fp);
  unsigned int b2 = getc(fp);
  unsigned int b3 = getc(fp);
  unsigned int b4 = getc(fp);

  return (b4 << 24) | (b3 << 16) | (b2 << 8) | b1;
}

static void DWordWriteLE(unsigned int x, FILE *fp)
{
  // Write a unsigned int to a file in little endian format
  unsigned char b1 = (x & 0x000000FF); putc(b1, fp);
  unsigned char b2 = ((x >> 8) & 0x000000FF); putc(b2, fp);
  unsigned char b3 = ((x >> 16) & 0x000000FF); putc(b3, fp);
  unsigned char b4 = ((x >> 24) & 0x000000FF); putc(b4, fp);
}

static int LongReadLE(FILE *fp)
{
  // Read a int word from a file in little endian format
  int b1 = getc(fp);
  int b2 = getc(fp);
  int b3 = getc(fp);
  int b4 = getc(fp);

  return (b4 << 24) | (b3 << 16) | (b2 << 8) | b1;
}

static void LongWriteLE(int x, FILE *fp)
{
  // Write a int to a file in little endian format
  char b1 = (x & 0x000000FF); putc(b1, fp);
  char b2 = ((x >> 8) & 0x000000FF); putc(b2, fp);
  char b3 = ((x >> 16) & 0x000000FF); putc(b3, fp);
  char b4 = ((x >> 24) & 0x000000FF); putc(b4, fp);
}

int Image::
ReadBMP(const char *filename)
{
  // Open file
  FILE *fp = fopen(filename, "rb");

  if (!fp) {
    fprintf(stderr, "Unable to open image file: %s", filename);
    return 0;
  }

  /* Read file header */
  BITMAPFILEHEADER bmfh;
  bmfh.bfType      = WordReadLE(fp);
  bmfh.bfSize      = DWordReadLE(fp);
  bmfh.bfReserved1 = WordReadLE(fp);
  bmfh.bfReserved2 = WordReadLE(fp);
  bmfh.bfOffBits   = DWordReadLE(fp);

  /* Check file header */
  assert(bmfh.bfType == BMP_BF_TYPE);

  /* ignore bmfh.bfSize */
  /* ignore bmfh.bfReserved1 */
  /* ignore bmfh.bfReserved2 */
  assert(bmfh.bfOffBits == BMP_BF_OFF_BITS);

  /* Read info header */
  BITMAPINFOHEADER bmih;
  bmih.biSize          = DWordReadLE(fp);
  bmih.biWidth         = LongReadLE(fp);
  bmih.biHeight        = LongReadLE(fp);
  bmih.biPlanes        = WordReadLE(fp);
  bmih.biBitCount      = WordReadLE(fp);
  bmih.biCompression   = DWordReadLE(fp);
  bmih.biSizeImage     = DWordReadLE(fp);
  bmih.biXPelsPerMeter = LongReadLE(fp);
  bmih.biYPelsPerMeter = LongReadLE(fp);
  bmih.biClrUsed       = DWordReadLE(fp);
  bmih.biClrImportant  = DWordReadLE(fp);

  // Check info header
  assert(bmih.biSize == BMP_BI_SIZE);
  assert(bmih.biWidth > 0);
  assert(bmih.biHeight > 0);
  assert(bmih.biPlanes == 1);
  assert(bmih.biBitCount == 24);        /* RGB */
  assert(bmih.biCompression == BI_RGB); /* RGB */
  int lineLength = bmih.biWidth * 3;    /* RGB */

  if ((lineLength % 4) != 0) lineLength = (lineLength / 4 + 1) * 4;
  assert(bmih.biSizeImage == (unsigned int)lineLength * (unsigned int)bmih.biHeight);

  // Assign width, height, and number of pixels
  width   = bmih.biWidth;
  height  = bmih.biHeight;
  npixels = width * height;

  // Allocate unsigned char buffer for reading pixels
  int rowsize = 3 * width;

  if ((rowsize % 4) != 0) rowsize = (rowsize / 4 + 1) * 4;
  int nbytes            = bmih.biSizeImage;
  unsigned char *buffer = new unsigned char [nbytes];

  if (!buffer) {
    fprintf(stderr, "Unable to allocate temporary memory for BMP file");
    fclose(fp);
    return 0;
  }

  // Read buffer
  fseek(fp, (long)bmfh.bfOffBits, SEEK_SET);

  if (fread(buffer, 1, bmih.biSizeImage, fp) != bmih.biSizeImage) {
    fprintf(stderr, "Error while reading BMP file %s", filename);
    return 0;
  }

  // Close file
  fclose(fp);

  // Allocate pixels for image
  pixels = new Pixel[width * height];

  if (!pixels) {
    fprintf(stderr, "Unable to allocate memory for BMP file");
    fclose(fp);
    return 0;
  }

  // Assign pixels
  for (int j = 0; j < height; j++) {
    unsigned char *p = &buffer[j * rowsize];

    for (int i = 0; i < width; i++) {
      double  b = (double)*(p++) / 255;
      double  g = (double)*(p++) / 255;
      double  r = (double)*(p++) / 255;
      Pixel pixel(r, g, b, 1);
      SetPixel(i, j, pixel);
    }
  }

  // Free unsigned char buffer for reading pixels
  delete[] buffer;

  // Return success
  return 1;
}

int Image::
WriteBMP(const char *filename) const
{
  // Open file
  FILE *fp = fopen(filename, "wb");

  if (!fp) {
    fprintf(stderr, "Unable to open image file: %s", filename);
    return 0;
  }

  // Compute number of bytes in row
  int rowsize = 3 * width;

  if ((rowsize % 4) != 0) rowsize = (rowsize / 4 + 1) * 4;

  // Write file header
  BITMAPFILEHEADER bmfh;
  bmfh.bfType      = BMP_BF_TYPE;
  bmfh.bfSize      = BMP_BF_OFF_BITS + rowsize * height;
  bmfh.bfReserved1 = 0;
  bmfh.bfReserved2 = 0;
  bmfh.bfOffBits   = BMP_BF_OFF_BITS;
  WordWriteLE(bmfh.bfType, fp);
  DWordWriteLE(bmfh.bfSize, fp);
  WordWriteLE(bmfh.bfReserved1, fp);
  WordWriteLE(bmfh.bfReserved2, fp);
  DWordWriteLE(bmfh.bfOffBits, fp);

  // Write info header
  BITMAPINFOHEADER bmih;
  bmih.biSize          = BMP_BI_SIZE;
  bmih.biWidth         = width;
  bmih.biHeight        = height;
  bmih.biPlanes        = 1;
  bmih.biBitCount      = 24;                                    /* RGB */
  bmih.biCompression   = BI_RGB;                                /* RGB */
  bmih.biSizeImage     = rowsize * (unsigned int)bmih.biHeight; /* RGB */
  bmih.biXPelsPerMeter = 2925;
  bmih.biYPelsPerMeter = 2925;
  bmih.biClrUsed       = 0;
  bmih.biClrImportant  = 0;
  DWordWriteLE(bmih.biSize, fp);
  LongWriteLE(bmih.biWidth,  fp);
  LongWriteLE(bmih.biHeight, fp);
  WordWriteLE(bmih.biPlanes,   fp);
  WordWriteLE(bmih.biBitCount, fp);
  DWordWriteLE(bmih.biCompression, fp);
  DWordWriteLE(bmih.biSizeImage,   fp);
  LongWriteLE(bmih.biXPelsPerMeter, fp);
  LongWriteLE(bmih.biYPelsPerMeter, fp);
  DWordWriteLE(bmih.biClrUsed,      fp);
  DWordWriteLE(bmih.biClrImportant, fp);

  // Write image, swapping blue and red in each pixel
  int pad = rowsize - width * 3;

  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      const Pixel& pixel = (*this)[i][j];
      double r             = 255.0 * pixel.Red();
      double g             = 255.0 * pixel.Green();
      double b             = 255.0 * pixel.Blue();

      if (r >= 255) r = 255;

      if (g >= 255) g = 255;

      if (b >= 255) b = 255;
      fputc((unsigned char)b, fp);
      fputc((unsigned char)g, fp);
      fputc((unsigned char)r, fp);
    }

    // Pad row
    for (int i = 0; i < pad; i++) fputc(0, fp);
  }

  // Close file
  fclose(fp);

  // Return success
  return 1;
}

////////////////////////////////////////////////////////////////////////
// PPM I/O
////////////////////////////////////////////////////////////////////////

int Image::
ReadPPM(const char *filename)
{
  // Open file
  FILE *fp = fopen(filename, "rb");

  if (!fp) {
    fprintf(stderr, "Unable to open image file: %s", filename);
    return 0;
  }

  // Read PPM file magic identifier
  char buffer[128];

  if (!fgets(buffer, 128, fp)) {
    fprintf(stderr, "Unable to read magic id in PPM file");
    fclose(fp);
    return 0;
  }

  // skip comments
  int c = getc(fp);

  while (c == '#') {
    while (c != '\n') c = getc(fp);
    c = getc(fp);
  }
  ungetc(c, fp);

  // Read width and height
  if (fscanf(fp, "%d%d", &width, &height) != 2) {
    fprintf(stderr, "Unable to read width and height in PPM file");
    fclose(fp);
    return 0;
  }

  // Read max value
  double max_value;

  if (fscanf(fp, "%lf", &max_value) != 1) {
    fprintf(stderr, "Unable to read max_value in PPM file");
    fclose(fp);
    return 0;
  }

  // Allocate image pixels
  pixels = new Pixel[width * height];

  if (!pixels) {
    fprintf(stderr, "Unable to allocate memory for PPM file");
    fclose(fp);
    return 0;
  }

  // Check if raw or ascii file
  if (!strcmp(buffer, "P6\n")) {
    // Read up to one character of whitespace (\n) after max_value
    int c = getc(fp);

    if (!isspace(c)) putc(c, fp);

    // Read raw image data
    // First ppm pixel is top-left, so read in opposite scan-line order
    for (int j = height - 1; j >= 0; j--) {
      for (int i = 0; i < width; i++) {
        double  r = (double)getc(fp) / max_value;
        double  g = (double)getc(fp) / max_value;
        double  b = (double)getc(fp) / max_value;
        Pixel pixel(r, g, b, 1);
        SetPixel(i, j, pixel);
      }
    }
  }
  else {
    // Read asci image data
    // First ppm pixel is top-left, so read in opposite scan-line order
    for (int j = height - 1; j >= 0; j--) {
      for (int i = 0; i < width; i++) {
        // Read pixel values
        int red, green, blue;

        if (fscanf(fp, "%d%d%d", &red, &green, &blue) != 3) {
          fprintf(stderr, "Unable to read data at (%d,%d) in PPM file", i, j);
          fclose(fp);
          return 0;
        }

        // Assign pixel values
        double  r = (double)red / max_value;
        double  g = (double)green / max_value;
        double  b = (double)blue / max_value;
        Pixel pixel(r, g, b, 1);
        SetPixel(i, j, pixel);
      }
    }
  }

  // Close file
  fclose(fp);

  // Return success
  return 1;
}

int Image::
WritePPM(const char *filename, int ascii) const
{
  // Check type
  if (ascii) {
    // Open file
    FILE *fp = fopen(filename, "w");

    if (!fp) {
      fprintf(stderr, "Unable to open image file: %s", filename);
      return 0;
    }

    // Print PPM image file
    // First ppm pixel is top-left, so write in opposite scan-line order
    fprintf(fp, "P3\n");
    fprintf(fp, "%d %d\n", width, height);
    fprintf(fp, "255\n");

    for (int j = height - 1; j >= 0; j--) {
      for (int i = 0; i < width; i++) {
        const Pixel& p = (*this)[i][j];
        int r            = (int)(255 * p.Red());
        int g            = (int)(255 * p.Green());
        int b            = (int)(255 * p.Blue());
        fprintf(fp, "%-3d %-3d %-3d  ", r, g, b);

        if (((i + 1) % 4) == 0) fprintf(fp, "\n");
      }

      if ((width % 4) != 0) fprintf(fp, "\n");
    }
    fprintf(fp, "\n");

    // Close file
    fclose(fp);
  }
  else {
    // Open file
    FILE *fp = fopen(filename, "wb");

    if (!fp) {
      fprintf(stderr, "Unable to open image file: %s", filename);
      return 0;
    }

    // Print PPM image file
    // First ppm pixel is top-left, so write in opposite scan-line order
    fprintf(fp, "P6\n");
    fprintf(fp, "%d %d\n", width, height);
    fprintf(fp, "255\n");

    for (int j = height - 1; j >= 0; j--) {
      for (int i = 0; i < width; i++) {
        const Pixel& p = (*this)[i][j];
        int r            = (int)(255 * p.Red());
        int g            = (int)(255 * p.Green());
        int b            = (int)(255 * p.Blue());
        fprintf(fp, "%c%c%c", r, g, b);
      }
    }

    // Close file
    fclose(fp);
  }

  // Return success
  return 1;
}

////////////////////////////////////////////////////////////////////////
// JPEG I/O
////////////////////////////////////////////////////////////////////////

int Image::
ReadJPEG(const char *filename)
{
  // Open file
  FILE *fp = fopen(filename, "rb");

  if (!fp) {
    fprintf(stderr, "Unable to open image file: %s", filename);
    return 0;
  }

  // Initialize decompression info
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, fp);
  jpeg_read_header(&cinfo, TRUE);
  jpeg_start_decompress(&cinfo);

  // Remember image attributes
  width   = cinfo.output_width;
  height  = cinfo.output_height;
  npixels = width * height;
  int ncomponents = cinfo.output_components;

  // Allocate pixels for image
  pixels = new Pixel[npixels];

  if (!pixels) {
    fprintf(stderr, "Unable to allocate memory for BMP file");
    fclose(fp);
    return 0;
  }

  // Allocate unsigned char buffer for reading image
  int rowsize = ncomponents * width;

  if ((rowsize % 4) != 0) rowsize = (rowsize / 4 + 1) * 4;
  int nbytes            = rowsize * height;
  unsigned char *buffer = new unsigned char [nbytes];

  if (!buffer) {
    fprintf(stderr, "Unable to allocate temporary memory for JPEG file");
    fclose(fp);
    return 0;
  }

  // Read scan lines
  // First jpeg pixel is top-left, so read pixels in opposite scan-line order
  while (cinfo.output_scanline < cinfo.output_height) {
    int scanline               = cinfo.output_height - cinfo.output_scanline - 1;
    unsigned char *row_pointer = &buffer[scanline * rowsize];
    jpeg_read_scanlines(&cinfo, &row_pointer, 1);
  }

  // Free everything
  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);

  // Close file
  fclose(fp);

  // Assign pixels
  for (int j = 0; j < height; j++) {
    unsigned char *p = &buffer[j * rowsize];

    for (int i = 0; i < width; i++) {
      double r, g, b, a;

      if (ncomponents == 1) {
        r = g = b = (double)*(p++) / 255;
        a = 1;
      }
      else if (ncomponents == 1) {
        r = g = b = (double)*(p++) / 255;
        a = 1;
        p++;
      }
      else if (ncomponents == 3) {
        r = (double)*(p++) / 255;
        g = (double)*(p++) / 255;
        b = (double)*(p++) / 255;
        a = 1;
      }
      else if (ncomponents == 4) {
        r = (double)*(p++) / 255;
        g = (double)*(p++) / 255;
        b = (double)*(p++) / 255;
        a = (double)*(p++) / 255;
      }
      else {
        fprintf(stderr, "Unrecognized number of components in jpeg image: %d\n", ncomponents);
        return 0;
      }
      Pixel pixel(r, g, b, a);
      SetPixel(i, j, pixel);
    }
  }

  // Free unsigned char buffer for reading pixels
  delete[] buffer;

  // Return success
  return 1;
}

int Image::
WriteJPEG(const char *filename) const
{
  // Open file
  FILE *fp = fopen(filename, "wb");

  if (!fp) {
    fprintf(stderr, "Unable to open image file: %s", filename);
    return 0;
  }

  // Initialize compression info
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  jpeg_stdio_dest(&cinfo, fp);
  cinfo.image_width      = width;   /* image width and height, in pixels */
  cinfo.image_height     = height;
  cinfo.input_components = 3;       /* # of color components per pixel */
  cinfo.in_color_space   = JCS_RGB; /* colorspace of input image */
  cinfo.dct_method       = JDCT_ISLOW;
  jpeg_set_defaults(&cinfo);
  cinfo.optimize_coding = TRUE;
  jpeg_set_quality(&cinfo, 75, TRUE);
  jpeg_start_compress(&cinfo, TRUE);

  // Allocate unsigned char buffer for reading image
  int rowsize = 3 * width;

  if ((rowsize % 4) != 0) rowsize = (rowsize / 4 + 1) * 4;
  int nbytes            = rowsize * height;
  unsigned char *buffer = new unsigned char [nbytes];

  if (!buffer) {
    fprintf(stderr, "Unable to allocate temporary memory for JPEG file");
    fclose(fp);
    return 0;
  }

  // Fill buffer with pixels
  for (int j = 0; j < height; j++) {
    unsigned char *p = &buffer[j * rowsize];

    for (int i = 0; i < width; i++) {
      const Pixel& pixel = (*this)[i][j];
      int r                = (int)(255 * pixel.Red());
      int g                = (int)(255 * pixel.Green());
      int b                = (int)(255 * pixel.Blue());

      if (r > 255) r = 255;

      if (g > 255) g = 255;

      if (b > 255) b = 255;
      *(p++) = r;
      *(p++) = g;
      *(p++) = b;
    }
  }


  // Output scan lines
  // First jpeg pixel is top-left, so write in opposite scan-line order
  while (cinfo.next_scanline < cinfo.image_height) {
    int scanline               = cinfo.image_height - cinfo.next_scanline - 1;
    unsigned char *row_pointer = &buffer[scanline * rowsize];
    jpeg_write_scanlines(&cinfo, &row_pointer, 1);
  }

  // Free everything
  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);

  // Close file
  fclose(fp);

  // Free unsigned char buffer for reading pixels
  delete[] buffer;

  // Return number of bytes written
  return 1;
}
