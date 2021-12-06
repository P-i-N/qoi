/*

QOI - The "Quite OK Image" format for fast, lossless image compression

Dominic Szablewski - https://phoboslab.org


-- LICENSE: The MIT License(MIT)

Copyright(c) 2021 Dominic Szablewski

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files(the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions :
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


-- About

QOI encodes and decodes images in a lossless format. An encoded QOI image is
usually around 10--30% larger than a decently optimized PNG image.

QOI outperforms simpler PNG encoders in compression ratio and performance. QOI
images are typically 20% smaller than PNGs written with stbi_image but 10%
larger than with libpng. Encoding is 25-50x faster and decoding is 3-4x faster
than stbi_image or libpng.


-- Synopsis

// Define `QOI_IMPLEMENTATION` in *one* C/C++ file before including this
// library to create the implementation.

#define QOI_IMPLEMENTATION
#include "qoi.h"

// Encode and store an RGBA buffer to the file system. The qoi_desc describes
// the input pixel data.
qoi_write("image_new.qoi", rgba_pixels, &(qoi_desc){
	.width = 1920,
	.height = 1080,
	.channels = 4,
	.colorspace = QOI_SRGB
});

// Load and decode a QOI image from the file system into a 32bbp RGBA buffer.
// The qoi_desc struct will be filled with the width, height, number of channels
// and colorspace read from the file header.
qoi_desc desc;
void *rgba_pixels = qoi_read("image.qoi", &desc, 4);



-- Documentation

This library provides the following functions;
- qoi_read    -- read and decode a QOI file
- qoi_decode  -- decode the raw bytes of a QOI image from memory
- qoi_write   -- encode and write a QOI file
- qoi_encode  -- encode an rgba buffer into a QOI image in memory

See the function declaration below for the signature and more information.

If you don't want/need the qoi_read and qoi_write functions, you can define
QOI_NO_STDIO before including this library.

This library uses malloc() and free(). To supply your own malloc implementation
you can define QOI_MALLOC and QOI_FREE before including this library.


-- Data Format

A QOI file has a 14 byte header, followed by any number of data "chunks".

struct qoi_header_t {
	char     magic[4];   // magic bytes "qoif"
	uint32_t width;      // image width in pixels (BE)
	uint32_t height;     // image height in pixels (BE)
	uint8_t  channels;   // must be 3 (RGB) or 4 (RGBA)
	uint8_t  colorspace; // a bitmap 0000rgba where
	                     //   - a zero bit indicates sRGBA,
	                     //   - a one bit indicates linear (user interpreted)
	                     //   colorspace for each channel
};

The decoder and encoder start with {r: 0, g: 0, b: 0, a: 255} as the previous
pixel value. Pixels are either encoded as
 - a run of the previous pixel
 - an index into a previously seen pixel
 - a difference to the previous pixel value in r,g,b,a
 - full r,g,b,a values

A running array[64] of previously seen pixel values is maintained by the encoder
and decoder. Each pixel that is seen by the encoder and decoder is put into this
array at the position (r^g^b^a) % 64. In the encoder, if the pixel value at this
index matches the current pixel, this index position is written to the stream.

Each chunk starts with a 2, 3 or 4 bit tag, followed by a number of data bits.
The bit length of chunks is divisible by 8 - i.e. all chunks are byte aligned.

QOI_INDEX {
	u8 tag  :  2;   // b00
	u8 idx  :  6;   // 6-bit index into the color index array: 0..63
}

QOI_DIFF_8 {
	u8 tag  :  2;   // b01
	u8 dr   :  2;   // 2-bit   red channel difference: -2..1
	u8 dg   :  2;   // 2-bit green channel difference: -2..1
	u8 db   :  2;   // 2-bit  blue channel difference: -2..1
}

QOI_DIFF_16 {
	u8 tag  :  3;   // b10
	u8 dr   :  5;   // 5-bit   red channel difference: -16..15
	u8 dg   :  5;   // 5-bit green channel difference: -16..15
	u8 db   :  4;   // 4-bit  blue channel difference:  -8.. 7
}

QOI_DIFF_16_A {
	u8 tag  :  3;   // b10
	u8 da   :  2;   // 2-bit  blue channel difference:  -2.. 1
	u8 dr   :  4;   // 4-bit   red channel difference:  -8..7
	u8 dg   :  4;   // 4-bit green channel difference:  -8.. 7
	u8 db   :  4;   // 4-bit  blue channel difference:  -8.. 7
}

QOI_RUN_8 {
	u8 tag  :  3;   // b110
	u8 run  :  5;   // 5-bit run-length (-1) repeating the previous pixel: 0..31, can be repeated
}

QOI_DIFF_24 {
	u8 tag  :  4;   // b1110
	u8 dr   :  7;   // 7-bit   red channel difference: -64..63
	u8 dg   :  7;   // 7-bit green channel difference: -64..63
	u8 db   :  6;   // 6-bit  blue channel difference: -32..31
}

QOI_DIFF_24_A {
	u8 tag  :  4;   // b1110
	u8 da   :  5;   // 5-bit alpha channel difference: -16..15
	u8 dr   :  5;   // 5-bit   red channel difference: -16..15
	u8 dg   :  5;   // 5-bit green channel difference: -16..15
	u8 db   :  5;   // 5-bit  blue channel difference: -16..15
}

QOI_COLOR {
	u8 tag  :  4;   // b1111
	u8 has_r:  1;   //   red byte follows
	u8 has_g:  1;   // green byte follows
	u8 has_b:  1;   //  blue byte follows
	u8 has_a:  1;   // alpha byte follows
	u8 r;           //   red value if has_r == 1: 0..255
	u8 g;           // green value if has_g == 1: 0..255
	u8 b;           //  blue value if has_b == 1: 0..255
	u8 a;           // alpha value if has_a == 1: 0..255
	// if mask is zero, this is not a color but a mode switch (color vs alpha)
}

The byte stream is padded with 4 zero bytes. Size the longest chunk we can
encounter is 5 bytes (QOI_COLOR with RGBA set), with this padding we just have
to check for an overrun once per decode loop iteration.

*/


// -----------------------------------------------------------------------------
// Header - Public functions

#ifndef QOI_H
#define QOI_H

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

// A pointer to qoi_desc struct has to be supplied to all of qoi's functions. It
// describes either the input format (for qoi_write, qoi_encode), or is filled
// with the description read from the file header (for qoi_read, qoi_decode).

// The colorspace in this qoi_desc is a bitmap with 0000rgba where a 0-bit
// indicates sRGB and a 1-bit indicates linear colorspace for each channel. You
// may use one of the predefined constants: QOI_SRGB, QOI_SRGB_LINEAR_ALPHA or
// QOI_LINEAR. The colorspace is purely informative. It will be saved to the
// file header, but does not affect en-/decoding in any way.

#define QOI_SRGB 0x00
#define QOI_SRGB_LINEAR_ALPHA 0x01
#define QOI_LINEAR 0x0f

#define QOI_COLOR_CACHE_SIZE 128

typedef struct
{
	unsigned int width;
	unsigned int height;
	unsigned char channels;
	unsigned char colorspace;
	int mode;
} qoi_desc;

typedef struct
{
	unsigned int count_hash_bucket[QOI_COLOR_CACHE_SIZE];
	unsigned int count_index;
	unsigned int count_diff_8;
	unsigned int count_diff_16;
	unsigned int count_run_8;
	unsigned int count_diff_24;
	unsigned int count_color;
} stats_t;

#ifndef QOI_NO_STDIO

	// Encode raw RGB or RGBA pixels into a QOI image and write it to the file
	// system. The qoi_desc struct must be filled with the image width, height,
	// number of channels (3 = RGB, 4 = RGBA) and the colorspace.

	// The function returns 0 on failure (invalid parameters, or fopen or malloc
	// failed) or the number of bytes written on success.

	int qoi_write( const char *filename, const void *data, const qoi_desc *desc );


	// Read and decode a QOI image from the file system. If channels is 0, the
	// number of channels from the file header is used. If channels is 3 or 4 the
	// output format will be forced into this number of channels.

	// The function either returns NULL on failure (invalid data, or malloc or fopen
	// failed) or a pointer to the decoded pixels. On success, the qoi_desc struct
	// will be filled with the description from the file header.

	// The returned pixel data should be free()d after use.

	void *qoi_read( const char *filename, qoi_desc *desc, int channels );

#endif // QOI_NO_STDIO


// Encode raw RGB or RGBA pixels into a QOI image in memory.

// The function either returns NULL on failure (invalid parameters or malloc
// failed) or a pointer to the encoded data on success. On success the out_len
// is set to the size in bytes of the encoded data.

// The returned qoi data should be free()d after user.

void *qoi_encode( const void *data, const qoi_desc *desc, int *out_len, stats_t *stats );


// Decode a QOI image from memory.

// The function either returns NULL on failure (invalid parameters or malloc
// failed) or a pointer to the decoded pixels. On success, the qoi_desc struct
// is filled with the description from the file header.

// The returned pixel data should be free()d after use.

void *qoi_decode( const void *data, int size, qoi_desc *desc, int channels );


#ifdef __cplusplus
}
#endif
#endif // QOI_H


// -----------------------------------------------------------------------------
// Implementation

#ifdef QOI_IMPLEMENTATION
#include <stdlib.h>

#ifndef QOI_MALLOC
	#define QOI_MALLOC(sz) (unsigned char *)malloc(sz)
	#define QOI_FREE(p)    free(p)
#endif

#define QOI_INDEX    0b00000000 // 0xxxxxxx
#define QOI_DIFF_8   0b10000000 // 10RRGGBB or 10xxxxxx
#define QOI_RUN_8    0b11000000 // 110xxxxx
#define QOI_DIFF_16  0b11100000 // 1110RRRR GGGGBBBB or 1110xxxx
#define QOI_DIFF_24  0b11110000 // 11110RRR RRRRGGGG GGBBBBBB
#define QOI_COLOR    0b11111000 // 11111xxx RRRRRRRR GGGGGGGG BBBBBBBB
#define QOI_COLOR_BW 0b11111001 // 11111001 LLLLLLLL
#define QOI_MODE_COL 0b11111100 // Switch to color mode
#define QOI_MODE_BW  0b11111101 // Switch to BW mode

#define QOI_MASK_1  0b10000000
#define QOI_MASK_2  0b11000000
#define QOI_MASK_3  0b11100000
#define QOI_MASK_4  0b11110000
#define QOI_MASK_5  0b11111000

#define QOI_COLOR_HASH(C) ( ( (C).rgba.r * 37 + (C).rgba.g ) * 37 + (C).rgba.b )
#define QOI_MAGIC \
	(((unsigned int)'q') << 24 | ((unsigned int)'i') << 16 | \
	 ((unsigned int)'x') <<  8 | ((unsigned int)'f'))
#define QOI_HEADER_SIZE 14
#define QOI_PADDING 4

#define QIX_RANGE(value, limit) ((value) >= -(limit) && (value) < (limit))

#define QOI_CHUNK_W 16
#define QOI_CHUNK_H 16
#define QOI_SEPARATE_COLUMNS
#define QOI_STATS(N) stats->N++

#ifndef QOI_STATS
	#define QOI_STATS(N)
#endif

#define QOI_SAVE_COLOR(C) index[QOI_COLOR_HASH(C) % QOI_COLOR_CACHE_SIZE] = C

typedef union
	{
	    struct { unsigned char r, g, b, a; } rgba;
	    unsigned int v;
	} qoi_rgba_t;

void qoi_write_32( unsigned char *bytes, size_t *p, unsigned int v )
{
	bytes[( *p )++] = ( 0xff000000 & v ) >> 24;
	bytes[( *p )++] = ( 0x00ff0000 & v ) >> 16;
	bytes[( *p )++] = ( 0x0000ff00 & v ) >> 8;
	bytes[( *p )++] = ( 0x000000ff & v );
}

unsigned int qoi_read_32( const unsigned char *bytes, size_t *p )
{
	unsigned int a = bytes[( *p )++];
	unsigned int b = bytes[( *p )++];
	unsigned int c = bytes[( *p )++];
	unsigned int d = bytes[( *p )++];
	return ( a << 24 ) | ( b << 16 ) | ( c << 8 ) | d;
}

typedef struct
{
	size_t width;        // Width in pixels
	size_t height;       // Height in pixels
	size_t channels;     // Number of channels (must be 3 or 4)
	size_t segment_size; // Segment size in pixels (0 = no segmentation)
	size_t stride;       // Line stride in bytes (usually == width * channels)
} image_t;

//---------------------------------------------------------------------------------------------------------------------
unsigned int *qix_zigzag_columns( const void *data, const image_t *image )
{
	unsigned int *result = ( unsigned int * )malloc( image->width * image->height * 4 );

	if ( image->channels == 4 )
	{
		unsigned int *dst = result;

		for ( size_t s = 0, S = ( image->width + image->segment_size - 1 ) / image->segment_size; s < S; ++s )
		{
			const unsigned int *src = ( const unsigned int * )data + s * image->segment_size;
			size_t sizeX = ( s < ( S - 1 ) ) ? image->segment_size : ( image->width - ( S - 1 ) * image->segment_size );

			for ( size_t y = 0, SY = image->height; y < SY; ++y )
			{
				_mm_prefetch((const char*)src + image->stride, _MM_HINT_T0);

				if ( y & 1 )
				{
					for ( size_t x = 0, nx = sizeX; x < sizeX; ++x )
						*dst++ = src[--nx];
				}
				else
				{
					memcpy( dst, src, sizeX * 4 );
					dst += sizeX;
				}

				src += image->stride / 4;
			}
		}
	}

	return result;
}

//---------------------------------------------------------------------------------------------------------------------
size_t qix_encode_rgb( const unsigned int *src, size_t numSrcPixels, unsigned char *dst, stats_t *stats )
{
	if ( !src || !numSrcPixels || !dst )
		return 0;

	stats_t empty_stats;
	if ( stats == NULL )
		stats = &empty_stats;

	memset( stats, 0, sizeof( stats_t ) );

	qoi_rgba_t index[QOI_COLOR_CACHE_SIZE] = { 0 };

	int run = 0;
	qoi_rgba_t px = { .rgba = {.r = 0, .g = 0, .b = 0, .a = 0 } };
	qoi_rgba_t pxYUV = px;
	qoi_rgba_t pxPrevYUV = px;

	unsigned char *bytes = ( unsigned char * )dst;

	for ( size_t i = 0; i < numSrcPixels; ++i )
	{
		bool sameAsPrev = px.v == src[0];
		bool flushRun = false;

		px.v = *src++;
		_mm_prefetch( ( const char * )src, _MM_HINT_T0 );

		if ( sameAsPrev )
		{
			run++;
			flushRun = ( i == numSrcPixels - 1 );
			QOI_STATS( count_run_8 );

			if ( !flushRun )
				continue;
		}
		else
			flushRun = run > 0;

		if ( flushRun )
		{
			unsigned char *start = bytes;
			--run;

			do
			{
				*bytes++ = QOI_RUN_8 | ( run & 0x1f );
				run >>= 5;
			}
			while ( run > 0 );

			// Swap to make big endian
			size_t len = ( bytes - start ) >> 1;
			for ( size_t i = 0; i < len; i++ )
			{
				unsigned char tmp = start[i];
				start[i] = bytes[-1 - i];
				bytes[-1 - i] = tmp;
			}

			run = 0;

			if ( sameAsPrev )
				continue;
		}

		pxPrevYUV = pxYUV;

		int Co = ( ( int )px.rgba.r - ( int )px.rgba.b ) / 2 + 128;
		int tmp = px.rgba.b + ( Co - 128 ) / 2;
		int Cg = ( px.rgba.g - tmp ) / 2 + 128;
		int Y = tmp + ( Cg - 128 );

		pxYUV.rgba.r = Y;
		pxYUV.rgba.g = Co;
		pxYUV.rgba.b = Cg;

		unsigned int index_pos = QOI_COLOR_HASH( pxYUV ) % QOI_COLOR_CACHE_SIZE;
		QOI_STATS( count_hash_bucket[index_pos] );

		if ( index[index_pos].v == pxYUV.v )
		{
			*bytes++ = index_pos;
			QOI_STATS( count_index );
		}
		else
		{
			index[index_pos] = pxYUV;

			int vr = pxYUV.rgba.r - pxPrevYUV.rgba.r;
			int vg = pxYUV.rgba.g - pxPrevYUV.rgba.g;
			int vb = pxYUV.rgba.b - pxPrevYUV.rgba.b;

			// Color mode
			if (
			    QIX_RANGE( vr, 64 ) &&
			    QIX_RANGE( vg, 32 ) && QIX_RANGE( vb, 32 )
			)
			{
				if (
				    QIX_RANGE( vr, 2 ) &&
				    QIX_RANGE( vg, 2 ) && QIX_RANGE( vb, 2 )
				)
				{
					*bytes++ = QOI_DIFF_8 | ( ( vr + 2 ) << 4 ) | ( vg + 2 ) << 2 | ( vb + 2 );
					QOI_STATS( count_diff_8 );
				}
				else if (
				    QIX_RANGE( vr, 8 ) &&
				    QIX_RANGE( vg, 8 ) && QIX_RANGE( vb, 8 )
				)
				{
					unsigned int value =
					    ( QOI_DIFF_16 << 8 ) | ( ( vr + 8 ) << 8 ) |
					    ( ( vg + 8 ) << 4 ) | ( vb + 8 );
					*bytes++ = ( unsigned char )( value >> 8 );
					*bytes++ = ( unsigned char )( value );
					QOI_STATS( count_diff_16 );
				}
				else
				{
					if ( pxYUV.rgba.g == 128 && pxYUV.rgba.b == 128 )
						goto encodecolor;

					unsigned int value =
					    ( QOI_DIFF_24 << 16 ) | ( ( vr + 64 ) << 12 ) |
					    ( ( vg + 32 ) << 6 ) | ( vb + 32 );

					*bytes++ = ( unsigned char )( value >> 16 );
					*bytes++ = ( unsigned char )( value >> 8 );
					*bytes++ = ( unsigned char )( value );
					QOI_STATS( count_diff_24 );
				}
			}
			else
			{
encodecolor:
				if ( pxYUV.rgba.g == 128 && pxYUV.rgba.b == 128 )
				{
					*bytes++ = QOI_COLOR_BW;
					*bytes++ = pxYUV.rgba.r;
				}
				else
				{
					*bytes++ = QOI_COLOR;
					*bytes++ = pxYUV.rgba.r;
					*bytes++ = pxYUV.rgba.g;
					*bytes++ = pxYUV.rgba.b;
				}
				QOI_STATS( count_color );
			}
		}
	}

	return bytes - dst;
}

//---------------------------------------------------------------------------------------------------------------------
void *qoi_encode( const void *data, const qoi_desc *desc, int *out_len, stats_t *stats )
{
	stats_t empty_stats;

	if ( stats == NULL )
		stats = &empty_stats;

	memset( stats, 0, sizeof( stats_t ) );

	if (
	    data == NULL || out_len == NULL || desc == NULL ||
	    desc->width == 0 || desc->height == 0 ||
	    desc->channels < 3 || desc->channels > 4 ||
	    ( desc->colorspace & 0xf0 ) != 0
	)
	{
		return NULL;
	}

	int max_size =
	    desc->width * desc->height * ( desc->channels + 1 ) +
	    QOI_HEADER_SIZE + QOI_PADDING;

	size_t p = 0;
	unsigned char *bytes = QOI_MALLOC( max_size );
	if ( !bytes )
	{
		return NULL;
	}

	qoi_write_32( bytes, &p, QOI_MAGIC );
	qoi_write_32( bytes, &p, desc->width );
	qoi_write_32( bytes, &p, desc->height );
	bytes[p++] = desc->channels;
	bytes[p++] = desc->colorspace;

	image_t img;
	img.width = desc->width;
	img.height = desc->height;
	img.stride = desc->width * desc->channels;
	img.channels = desc->channels;
	img.segment_size = QOI_CHUNK_W;

	unsigned int *zigzagData = qix_zigzag_columns( data, &img );
	//unsigned int *zigzagData = ( unsigned int * )data;

	for ( size_t s = 0, S = ( img.width + img.segment_size - 1 ) / img.segment_size; s < S; ++s )
	{
		const unsigned int *src = zigzagData + s * img.segment_size * img.height;
		size_t sizeX = ( s < ( S - 1 ) ) ? img.segment_size : ( img.width - ( S - 1 ) * img.segment_size );

		stats_t column_stats = { 0 };
		p += qix_encode_rgb( src, sizeX * img.height, bytes + p, &column_stats );

		stats->count_index += column_stats.count_index;
		stats->count_diff_8 += column_stats.count_diff_8;
		stats->count_diff_16 += column_stats.count_diff_16;
		stats->count_run_8 += column_stats.count_run_8;
		stats->count_diff_24 += column_stats.count_diff_24;
		stats->count_color += column_stats.count_color;
	}

	if ( zigzagData != data )
		free( zigzagData );

	for ( int i = 0; i < QOI_PADDING; i++ )
		bytes[p++] = 0;

	*out_len = ( int )p;
	return bytes;
}

void *qoi_decode( const void *data, int size, qoi_desc *desc, int channels )
{
	if (
	    data == NULL || desc == NULL ||
	    ( channels != 0 && channels != 3 && channels != 4 ) ||
	    size < QOI_HEADER_SIZE + QOI_PADDING
	)
	{
		return NULL;
	}

	const unsigned char *bytes = ( const unsigned char * )data;
	size_t p = 0;

	unsigned int header_magic = qoi_read_32( bytes, &p );
	desc->width = qoi_read_32( bytes, &p );
	desc->height = qoi_read_32( bytes, &p );
	desc->channels = bytes[p++];
	desc->colorspace = bytes[p++];

	if (
	    desc->width == 0 || desc->height == 0 ||
	    desc->channels < 3 || desc->channels > 4 ||
	    header_magic != QOI_MAGIC
	)
	{
		return NULL;
	}

	if ( channels == 0 )
	{
		channels = desc->channels;
	}

	int px_len = desc->width * desc->height * channels;
	unsigned char *pixels = QOI_MALLOC( px_len );
	if ( !pixels )
	{
		return NULL;
	}

	qoi_rgba_t px = {.rgba = {.r = 0, .g = 0, .b = 0, .a = 255}};
	qoi_rgba_t index[QOI_COLOR_CACHE_SIZE] = { 0 };
	qoi_rgba_t pxRGB = { 0 };

	int run = 0;
	int mode = 0;
	int chunks_len = size - QOI_PADDING;

	int chunks_x_count = desc->width / QOI_CHUNK_W;

	for ( int chunk_x = 0; chunk_x < chunks_x_count; chunk_x++ )
	{
		int x_pixels = QOI_CHUNK_W;
		if ( chunk_x == chunks_x_count - 1 )
		{
			x_pixels = desc->width - ( chunks_x_count - 1 ) * QOI_CHUNK_W;
		}

#ifdef QOI_SEPARATE_COLUMNS
		memset( index, 0, sizeof( qoi_rgba_t ) * QOI_COLOR_CACHE_SIZE );
		run = 0;
		px.rgba.r = 0;
		px.rgba.g = 0;
		px.rgba.b = 0;
		px.rgba.a = 255;
		pxRGB = px;
		pxRGB.rgba.a = 0;
		mode = 0;
#endif

		int px_chunk_pos = chunk_x * QOI_CHUNK_W;
		unsigned char *px_ptr = pixels + px_chunk_pos * channels;

		for ( int y = 0, inc = channels, SY = desc->height; y < SY; y++, px_chunk_pos += desc->width, inc *= -1 )
		{
			for ( int x = 0; x < x_pixels; x++, px_ptr += inc )
			{
				if ( run > 0 )
				{
					run--;
				}
				else if ( p < chunks_len )
				{
					int b1 = bytes[p++];

					if ( ( b1 & QOI_MASK_1 ) == QOI_INDEX )
					{
						px = index[b1];
					}
					else if ( ( b1 & QOI_MASK_3 ) == QOI_RUN_8 )
					{
						run = b1 & 0x1f;
						while ( p < chunks_len && ( ( b1 = bytes[p] ) & QOI_MASK_3 ) == QOI_RUN_8 )
						{
							p++;
							run <<= 5;
							run += b1 & 0x1f;
						}
						// no need to increment here, one implied copy
					}
					else if ( ( b1 & QOI_MASK_2 ) == QOI_DIFF_8 )
					{
						px.rgba.r += ( ( b1 >> 4 ) & 0x03 ) - 2;
						px.rgba.g += ( ( b1 >> 2 ) & 0x03 ) - 2;
						px.rgba.b += ( b1       & 0x03 ) - 2;
						QOI_SAVE_COLOR( px );
					}
					else if ( ( b1 & QOI_MASK_4 ) == QOI_DIFF_16 )
					{
						b1 = ( b1 << 8 ) + bytes[p++];

						if ( mode == 0 )
						{
							px.rgba.r += ( ( b1 >> 8 ) & 0x0f ) - 8;
							px.rgba.g += ( ( b1 >> 4 ) & 0x0f ) - 8;
							px.rgba.b += ( b1 & 0x0f ) - 8;
						}
						else
						{
							px.rgba.r += ( ( b1 >> 8 ) & 0x0f ) - 8;
							px.rgba.g += ( ( b1 >> 4 ) & 0x0f ) - 8;
							px.rgba.b += ( b1 & 0x0f ) - 8;
							px.rgba.a += ( ( b1 >> 12 ) & 0x03 ) - 2;
						}

						QOI_SAVE_COLOR( px );
					}
					else if ( ( b1 & QOI_MASK_5 ) == QOI_DIFF_24 )
					{
						b1 <<= 16;
						b1 |= bytes[p++] << 8;
						b1 |= bytes[p++];

						if ( mode == 0 )
						{
							px.rgba.r += ( ( b1 >> 12 ) & 0x7f ) - 64;
							px.rgba.g += ( ( b1 >> 6 ) & 0x3f ) - 32;
							px.rgba.b += ( b1 & 0x3f ) - 32;
						}
						else
						{
							px.rgba.r += ( ( b1 >> 10 ) & 0x1f ) - 16;
							px.rgba.g += ( ( b1 >> 5 ) & 0x1f ) - 16;
							px.rgba.b += ( b1 & 0x1f ) - 16;
							px.rgba.a += ( ( b1 >> 15 ) & 0x1f ) - 16;
						}

						QOI_SAVE_COLOR( px );
					}
					else if ( ( b1 & QOI_MASK_5 ) == QOI_COLOR )
					{
						if ( b1 == QOI_COLOR_BW )
						{
							px.rgba.r = bytes[p++];
							px.rgba.g = px.rgba.b = 128;
						}
						else
						{
							px.rgba.r = bytes[p++];
							px.rgba.g = bytes[p++];
							px.rgba.b = bytes[p++];
						}
						QOI_SAVE_COLOR( px );
					}

					int tmp = ( int )px.rgba.r - ( ( int )px.rgba.b - 128 );
					pxRGB.rgba.g = 2 * ( ( int )px.rgba.b - 128 ) + tmp;
					pxRGB.rgba.b = tmp - ( ( int )px.rgba.g - 128 ) / 2;
					pxRGB.rgba.r = pxRGB.rgba.b + 2 * ( ( int )px.rgba.g - 128 );
					pxRGB.rgba.a = px.rgba.a;
				}

				if ( channels == 4 )
				{
					memcpy( px_ptr, &pxRGB, 4 );
				}
				else
				{
					px_ptr[0] = pxRGB.rgba.r;
					px_ptr[1] = pxRGB.rgba.g;
					px_ptr[2] = pxRGB.rgba.b;
				}
			}

			px_ptr += desc->width * desc->channels - inc;
		}
	}

	return pixels;
}

#ifndef QOI_NO_STDIO
#include <stdio.h>

int qoi_write( const char *filename, const void *data, const qoi_desc *desc )
{
	int size;
	void *encoded = qoi_encode( data, desc, &size, NULL );
	if ( !encoded )
	{
		return 0;
	}

	FILE *f = fopen( filename, "wb" );
	if ( !f )
	{
		QOI_FREE( encoded );
		return 0;
	}

	fwrite( encoded, 1, size, f );
	fclose( f );
	QOI_FREE( encoded );
	return size;
}

void *qoi_read( const char *filename, qoi_desc *desc, int channels )
{
	FILE *f = fopen( filename, "rb" );
	if ( !f )
	{
		return NULL;
	}

	fseek( f, 0, SEEK_END );
	int size = ftell( f );
	fseek( f, 0, SEEK_SET );

	void *data = QOI_MALLOC( size );
	if ( !data )
	{
		fclose( f );
		return NULL;
	}

	int bytes_read = ( int )fread( data, 1, size, f );
	fclose( f );

	void *pixels = qoi_decode( data, bytes_read, desc, channels );
	QOI_FREE( data );
	return pixels;
}

#endif // QOI_NO_STDIO
#endif // QOI_IMPLEMENTATION
