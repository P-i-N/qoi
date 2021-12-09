// -----------------------------------------------------------------------------
// Header - Public functions
#ifndef QIX_H
#define QIX_H

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

// A pointer to qix_desc struct has to be supplied to all of qoi's functions. It
// describes either the input format (for qix_write, qix_encode), or is filled
// with the description read from the file header (for qix_read, qix_decode).

// The colorspace in this qix_desc is a bitmap with 0000rgba where a 0-bit
// indicates sRGB and a 1-bit indicates linear colorspace for each channel. You
// may use one of the predefined constants: QIX_SRGB, QIX_SRGB_LINEAR_ALPHA or
// QIX_LINEAR. The colorspace is purely informative. It will be saved to the
// file header, but does not affect en-/decoding in any way.

#define QIX_SRGB 0x00
#define QIX_SRGB_LINEAR_ALPHA 0x01
#define QIX_LINEAR 0x0f

#define QIX_COLOR_CACHE_SIZE 128
#define QIX_COLOR_CACHE2_SIZE 1024
#define QIX_LRU_CACHE_SIZE 7

typedef struct
{
	unsigned int width;
	unsigned int height;
	unsigned char channels;
	unsigned char colorspace;
	int mode;
} qix_desc;

typedef struct
{
	unsigned int count_hash_bucket[QIX_COLOR_CACHE_SIZE];
	unsigned int count_index;
	unsigned int count_index2;
	unsigned int count_diff_8;
	unsigned int count_diff_16;
	unsigned int count_run_8;
	unsigned int count_diff_24;
	unsigned int count_color;
} stats_t;

#ifndef QIX_NO_STDIO

	// Encode raw RGB or RGBA pixels into a QOI image and write it to the file
	// system. The qix_desc struct must be filled with the image width, height,
	// number of channels (3 = RGB, 4 = RGBA) and the colorspace.

	// The function returns 0 on failure (invalid parameters, or fopen or malloc
	// failed) or the number of bytes written on success.

	int qix_write( const char *filename, const void *data, const qix_desc *desc );


	// Read and decode a QOI image from the file system. If channels is 0, the
	// number of channels from the file header is used. If channels is 3 or 4 the
	// output format will be forced into this number of channels.

	// The function either returns NULL on failure (invalid data, or malloc or fopen
	// failed) or a pointer to the decoded pixels. On success, the qix_desc struct
	// will be filled with the description from the file header.

	// The returned pixel data should be free()d after use.

	void *qix_read( const char *filename, qix_desc *desc, int channels );

#endif // QIX_NO_STDIO


// Encode raw RGB or RGBA pixels into a QOI image in memory.

// The function either returns NULL on failure (invalid parameters or malloc
// failed) or a pointer to the encoded data on success. On success the out_len
// is set to the size in bytes of the encoded data.

// The returned qoi data should be free()d after user.

void *qix_encode( const void *data, const qix_desc *desc, int *out_len, stats_t *stats );


// Decode a QOI image from memory.

// The function either returns NULL on failure (invalid parameters or malloc
// failed) or a pointer to the decoded pixels. On success, the qix_desc struct
// is filled with the description from the file header.

// The returned pixel data should be free()d after use.

void *qix_decode( const void *data, int size, qix_desc *desc, int channels );


#ifdef __cplusplus
}
#endif
#endif // QIX_H


// -----------------------------------------------------------------------------
// Implementation

#ifdef QIX_IMPLEMENTATION
#include <stdlib.h>

#ifndef QIX_MALLOC
	#define QIX_MALLOC(sz) (unsigned char *)malloc(sz)
	#define QIX_FREE(p)    free(p)
#endif

#define QIX_INDEX    0b00000000 // 0xxxxxxx
#define QIX_DIFF_8   0b10000000 // 10xxxxxx
#define QIX_RUN_8    0b11000000 // 110xxxxx
#define QIX_DIFF_16  0b11100000 // 1110RRRR GGGGBBBB
#define QIX_DIFF_24  0b11110000 // 11110RRR RRRRGGGG GGBBBBBB
#define QIX_COLOR    0b11111000 // 11111000 RRRRRRRR GGGGGGGG BBBBBBBB
#define QIX_COLOR_Y  0b11111001 // 11111001 YYYYYYYY
#define QIX_COLOR_BW 0b11111010 // 11111010 YYYYYYYY
#define QIX_INDEX_16 0b11111100 // 111111xx xxxxxxxx

#define QIX_MASK_1  0b10000000
#define QIX_MASK_2  0b11000000
#define QIX_MASK_3  0b11100000
#define QIX_MASK_4  0b11110000
#define QIX_MASK_5  0b11111000
#define QIX_MASK_6  0b11111100

#define QIX_COLOR_HASH(C) ( ( (C).r * 37 + (C).g ) * 37 + (C).b )
#define QIX_MAGIC \
	(((unsigned int)'q') << 24 | ((unsigned int)'i') << 16 | \
	 ((unsigned int)'x') <<  8 | ((unsigned int)'f'))
#define QIX_HEADER_SIZE 14
#define QIX_PADDING 4

#define QIX_RANGE(value, limit) ((value) >= -(limit) && (value) < (limit))
#define QIX_RANGE_EX(value, limit) ((value) >= -(limit) && (value) <= (limit))

#define QIX_SEGMENT_SIZE 16
#define QIX_SEPARATE_COLUMNS
//#define QIX_STATS(N) stats->N++

#ifndef QIX_STATS
	#define QIX_STATS(N)
	#define QIX_NO_STATS
#endif

#define QIX_SAVE_COLOR(C) \
	index[(QIX_COLOR_HASH(C) % QIX_COLOR_CACHE2_SIZE) % QIX_COLOR_CACHE_SIZE] = C; \
	index2[QIX_COLOR_HASH(C) % QIX_COLOR_CACHE2_SIZE] = C

typedef union
	{
	    struct { unsigned char r, g, b, a; } rgba;
	    unsigned int cg;
	} qix_rgba_t;

typedef struct
{
	union
	{
		struct
		{
			unsigned char r, g, b, a;
		};

		unsigned int rgba;
	};

	int y, co, cg;
} qix_rgb_yuv;

void qix_write_32( unsigned char *bytes, size_t *p, unsigned int v )
{
	bytes[( *p )++] = ( 0xff000000 & v ) >> 24;
	bytes[( *p )++] = ( 0x00ff0000 & v ) >> 16;
	bytes[( *p )++] = ( 0x0000ff00 & v ) >> 8;
	bytes[( *p )++] = ( 0x000000ff & v );
}

unsigned int qix_read_32( const unsigned char *bytes, size_t *p )
{
	unsigned int a = bytes[( *p )++];
	unsigned int b = bytes[( *p )++];
	unsigned int c = bytes[( *p )++];
	unsigned int d = bytes[( *p )++];
	return ( a << 24 ) | ( b << 16 ) | ( c << 8 ) | d;
}

void qix_rgb2yuv( qix_rgb_yuv *pix )
{
	pix->co = pix->r - pix->b;
	int tmp = pix->b + pix->co / 2;
	pix->cg = pix->g - tmp;
	pix->y = tmp + pix->cg / 2;
}

void qix_yuv2rgb( qix_rgb_yuv *pix )
{
	int tmp = pix->y - pix->cg / 2;
	pix->g = pix->cg + tmp;
	pix->b = tmp - pix->co / 2;
	pix->r = pix->b + pix->co;
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

	if ( result && image->channels == 4 )
	{
		unsigned int *dst = result;

		for ( size_t s = 0, S = ( image->width + image->segment_size - 1 ) / image->segment_size; s < S; ++s )
		{
			const unsigned int *src = ( const unsigned int * )data + s * image->segment_size;
			size_t sizeX = ( s < ( S - 1 ) ) ? image->segment_size : ( image->width - ( S - 1 ) * image->segment_size );

			for ( size_t y = 0, SY = image->height; y < SY; ++y )
			{
				_mm_prefetch( ( const char * )src + image->stride, _MM_HINT_T0 );

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

	int run = 0;

	unsigned int index[QIX_COLOR_CACHE_SIZE] = { 0 };
	unsigned int index2[QIX_COLOR_CACHE2_SIZE] = { 0 };
	qix_rgb_yuv px = { 0 };
	qix_rgb_yuv pxPrev = { 0 };

	unsigned char *bytes = ( unsigned char * )dst;

	static int column = 0;
	++column;

	int pixelIndex = 0;

	while ( numSrcPixels-- )
	{
		++pixelIndex;

		px.rgba = ( *src++ ) & 0xFFFFFF;
		bool sameAsPrev = !memcmp( &px.rgba, &pxPrev.rgba, 4 );
		bool flushRun = false;

		if ( sameAsPrev )
		{
			run++;
			flushRun = ( numSrcPixels == 0 );
			QIX_STATS( count_run_8 );

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
				*bytes++ = QIX_RUN_8 | ( run & 0x1f );
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

		pxPrev = px;
		qix_rgb2yuv( &px );

		unsigned int index_pos2 = QIX_COLOR_HASH( px ) % QIX_COLOR_CACHE2_SIZE;
		unsigned int index_pos = index_pos2 % QIX_COLOR_CACHE_SIZE;
		QIX_STATS( count_hash_bucket[index_pos] );

		if ( index[index_pos] == px.rgba )
		{
			*bytes++ = index_pos;
			QIX_STATS( count_index );
			continue;
		}

		int vg = ( px.co - pxPrev.co );
		int vb = ( px.cg - pxPrev.cg );

		bool encodeColor = false;

		if ( QIX_RANGE( vg, 32 ) && QIX_RANGE( vb, 32 ) )
		{
			int vr = px.y - pxPrev.y;

			if ( QIX_RANGE_EX( vr, 3 ) && QIX_RANGE_EX( vg, 1 ) && QIX_RANGE_EX( vb, 1 ) )
			{
				// Y range <-3; +3>; U range <-1; +1>; V range <-1; +1>
				*bytes++ = QIX_DIFF_8 | ( 9 * ( vr + 3 ) + 3 * ( vg + 1 ) + ( vb + 1 ) );
				QIX_STATS( count_diff_8 );
			}
			else if ( vg == 0 && vb == 0 )
			{
				*bytes++ = QIX_COLOR_Y;
				*bytes++ = px.y;
				QIX_STATS( count_diff_16 );
			}
			else if ( px.co == 0 && px.cg == 0 )
			{
				*bytes++ = QIX_COLOR_BW;
				*bytes++ = px.y;
				QIX_STATS( count_diff_16 );
			}
			else if ( index2[index_pos2] == px.rgba )
			{
				unsigned int value = ( QIX_INDEX_16 << 8 ) | ( index_pos2 );
				*bytes++ = ( unsigned char )( value >> 8 );
				*bytes++ = ( unsigned char )( value );
				QIX_STATS( count_index2 );
			}
			else if ( QIX_RANGE( vr, 8 ) && QIX_RANGE( vg, 8 ) && QIX_RANGE( vb, 8 ) )
			{
				unsigned int value =
				    ( QIX_DIFF_16 << 8 ) | ( ( vr + 8 ) << 8 ) |
				    ( ( vg + 8 ) << 4 ) | ( vb + 8 );
				*bytes++ = ( unsigned char )( value >> 8 );
				*bytes++ = ( unsigned char )( value );
				QIX_STATS( count_diff_16 );
			}
			else if ( QIX_RANGE( vr, 64 ) && QIX_RANGE( vg, 32 ) && QIX_RANGE( vb, 32 ) )
			{
				unsigned int value =
				    ( QIX_DIFF_24 << 16 ) | ( ( vr + 64 ) << 12 ) |
				    ( ( vg + 32 ) << 6 ) | ( vb + 32 );

				*bytes++ = ( unsigned char )( value >> 16 );
				*bytes++ = ( unsigned char )( value >> 8 );
				*bytes++ = ( unsigned char )( value );
				QIX_STATS( count_diff_24 );
			}
			else
				encodeColor = true;
		}
		else
			encodeColor = true;

		if ( encodeColor )
		{
			if ( px.co == 0 && px.cg == 0 )
			{
				*bytes++ = QIX_COLOR_BW;
				*bytes++ = px.y;
				QIX_STATS( count_diff_16 );
			}
			else
			{
				if ( index2[index_pos2] == px.rgba )
				{
					unsigned int value = ( QIX_INDEX_16 << 8 ) | ( index_pos2 );
					*bytes++ = ( unsigned char )( value >> 8 );
					*bytes++ = ( unsigned char )( value );
					QIX_STATS( count_index2 );
					continue;
				}
				else
				{
					*bytes++ = QIX_COLOR;
					*bytes++ = ( unsigned char )( px.rgba );
					*bytes++ = ( unsigned char )( px.rgba >> 8 );
					*bytes++ = ( unsigned char )( px.rgba >> 16 );
					QIX_STATS( count_color );
				}
			}
		}

		index[index_pos] = px.rgba;
		index2[index_pos2] = px.rgba;
	}

	return bytes - dst;
}

//---------------------------------------------------------------------------------------------------------------------
void *qix_encode( const void *data, const qix_desc *desc, int *out_len, stats_t *stats )
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
	    QIX_HEADER_SIZE + QIX_PADDING;

	size_t p = 0;
	unsigned char *bytes = QIX_MALLOC( max_size );
	if ( !bytes )
	{
		return NULL;
	}

	qix_write_32( bytes, &p, QIX_MAGIC );
	qix_write_32( bytes, &p, desc->width );
	qix_write_32( bytes, &p, desc->height );
	bytes[p++] = desc->channels;
	bytes[p++] = desc->colorspace;

	image_t img;
	img.width = desc->width;
	img.height = desc->height;
	img.stride = desc->width * desc->channels;
	img.channels = desc->channels;
	img.segment_size = QIX_SEGMENT_SIZE;

	unsigned int *zigzagData = ( unsigned int * )data;
	zigzagData = qix_zigzag_columns( data, &img );

	for ( size_t s = 0, S = ( img.width + img.segment_size - 1 ) / img.segment_size; s < S; ++s )
	{
		const unsigned int *src = zigzagData + s * img.segment_size * img.height;
		size_t sizeX = ( s < ( S - 1 ) ) ? img.segment_size : ( img.width - ( S - 1 ) * img.segment_size );

		stats_t column_stats = { 0 };
		p += qix_encode_rgb( src, sizeX * img.height, bytes + p, &column_stats );

		stats->count_index += column_stats.count_index;
		stats->count_index2 += column_stats.count_index2;
		stats->count_diff_8 += column_stats.count_diff_8;
		stats->count_diff_16 += column_stats.count_diff_16;
		stats->count_run_8 += column_stats.count_run_8;
		stats->count_diff_24 += column_stats.count_diff_24;
		stats->count_color += column_stats.count_color;
	}

	if ( zigzagData != data )
		free( zigzagData );

	for ( int i = 0; i < QIX_PADDING; i++ )
		bytes[p++] = 0;

	*out_len = ( int )p;
	return bytes;
}

//---------------------------------------------------------------------------------------------------------------------
void *qix_decode( const void *data, int size, qix_desc *desc, int channels )
{
	if (
	    data == NULL || desc == NULL ||
	    ( channels != 0 && channels != 3 && channels != 4 ) ||
	    size < QIX_HEADER_SIZE + QIX_PADDING
	)
	{
		return NULL;
	}

	const unsigned char *bytes = ( const unsigned char * )data;
	size_t p = 0;

	unsigned int header_magic = qix_read_32( bytes, &p );
	desc->width = qix_read_32( bytes, &p );
	desc->height = qix_read_32( bytes, &p );
	desc->channels = bytes[p++];
	desc->colorspace = bytes[p++];

	if (
	    desc->width == 0 || desc->height == 0 ||
	    desc->channels < 3 || desc->channels > 4 ||
	    header_magic != QIX_MAGIC
	)
	{
		return NULL;
	}

	if ( channels == 0 )
	{
		channels = desc->channels;
	}

	int px_len = desc->width * desc->height * channels;
	unsigned char *pixels = QIX_MALLOC( px_len );
	if ( !pixels )
	{
		return NULL;
	}

	qix_rgb_yuv index[QIX_COLOR_CACHE_SIZE] = { 0 };
	qix_rgb_yuv index2[QIX_COLOR_CACHE2_SIZE] = { 0 };
	qix_rgb_yuv px = { 0 };

	int run = 0;
	int chunks_len = size - QIX_PADDING;
	int chunks_x_count = desc->width / QIX_SEGMENT_SIZE;

	for ( int chunk_x = 0; chunk_x < chunks_x_count; chunk_x++ )
	{
		int x_pixels = QIX_SEGMENT_SIZE;
		if ( chunk_x == chunks_x_count - 1 )
		{
			x_pixels = desc->width - ( chunks_x_count - 1 ) * QIX_SEGMENT_SIZE;
		}

		memset( index, 0, sizeof( qix_rgb_yuv ) * QIX_COLOR_CACHE_SIZE );
		memset( index2, 0, sizeof( qix_rgb_yuv ) * QIX_COLOR_CACHE2_SIZE );
		run = 0;
		px.rgba = 0xFF000000u;
		px.y = px.co = px.cg = 0;

		int px_chunk_pos = chunk_x * QIX_SEGMENT_SIZE;
		unsigned char *px_ptr = pixels + px_chunk_pos * channels;

		for ( int y = 0, inc = channels, SY = desc->height; y < SY; y++, px_chunk_pos += desc->width, inc *= -1 )
		{
			for ( int x = 0; x < x_pixels; x++, px_ptr += inc )
			{
				if ( run > 0 )
				{
					run--;
				}
				else
				{
					int b1 = bytes[p++];

					if ( b1 == QIX_COLOR_Y )
					{
						px.y = bytes[p++];
					}
					else if ( b1 == QIX_COLOR_BW )
					{
						px.y = bytes[p++];
						px.co = 0;
						px.cg = 0;
					}
					else if ( ( b1 & QIX_MASK_1 ) == QIX_INDEX )
					{
						px = index[b1];
					}
					else if ( ( b1 & QIX_MASK_3 ) == QIX_RUN_8 )
					{
						run = b1 & 0x1f;
						while ( ( ( b1 = bytes[p] ) & QIX_MASK_3 ) == QIX_RUN_8 )
						{
							p++;
							run <<= 5;
							run += b1 & 0x1f;
						}
					}
					else if ( ( b1 & QIX_MASK_2 ) == QIX_DIFF_8 )
					{
						int value = b1 & ~QIX_MASK_2;
						px.y += ( value / 9 ) - 3;
						px.co += ( ( value / 3 ) % 3 ) - 1;
						px.cg += ( value % 3 ) - 1;
					}
					else if ( ( b1 & QIX_MASK_4 ) == QIX_DIFF_16 )
					{
						b1 = ( b1 << 8 ) + bytes[p++];
						px.y += ( ( b1 >> 8 ) & 0x0f ) - 8;
						px.co += ( ( b1 >> 4 ) & 0x0f ) - 8;
						px.cg += ( b1 & 0x0f ) - 8;
					}
					else if ( ( b1 & QIX_MASK_5 ) == QIX_DIFF_24 )
					{
						b1 <<= 16;
						b1 |= bytes[p++] << 8;
						b1 |= bytes[p++];

						px.y += ( ( b1 >> 12 ) & 0x7f ) - 64;
						px.co += ( ( b1 >> 6 ) & 0x3f ) - 32;
						px.cg += ( b1 & 0x3f ) - 32;
					}
					else if ( ( b1 & QIX_MASK_6 ) == QIX_INDEX_16 )
					{
						b1 = ( b1 << 8 ) + bytes[p++];
						px = index2[b1 & 0x3ff];
					}
					else if ( b1 == QIX_COLOR )
					{
						px.r = bytes[p++];
						px.g = bytes[p++];
						px.b = bytes[p++];
						px.a = 255;
						qix_rgb2yuv( &px );
					}

					qix_yuv2rgb( &px );
					unsigned int index_pos2 = QIX_COLOR_HASH( px ) % QIX_COLOR_CACHE2_SIZE;
					unsigned int index_pos = index_pos2 % QIX_COLOR_CACHE_SIZE;
					index[index_pos] = px;
					index2[index_pos2] = px;
				}

				if ( channels == 4 )
				{
					memcpy( px_ptr, &px.rgba, 4 );
				}
				else
				{
					px_ptr[0] = px.r;
					px_ptr[1] = px.g;
					px_ptr[2] = px.b;
				}
			}

			px_ptr += desc->width * desc->channels - inc;
		}
	}

	return pixels;
}

#ifndef QIX_NO_STDIO
#include <stdio.h>

int qix_write( const char *filename, const void *data, const qix_desc *desc )
{
	int size;
	void *encoded = qix_encode( data, desc, &size, NULL );
	if ( !encoded )
	{
		return 0;
	}

	FILE *f = fopen( filename, "wb" );
	if ( !f )
	{
		QIX_FREE( encoded );
		return 0;
	}

	fwrite( encoded, 1, size, f );
	fclose( f );
	QIX_FREE( encoded );
	return size;
}

void *qix_read( const char *filename, qix_desc *desc, int channels )
{
	FILE *f = fopen( filename, "rb" );
	if ( !f )
	{
		return NULL;
	}

	fseek( f, 0, SEEK_END );
	int size = ftell( f );
	fseek( f, 0, SEEK_SET );

	void *data = QIX_MALLOC( size );
	if ( !data )
	{
		fclose( f );
		return NULL;
	}

	int bytes_read = ( int )fread( data, 1, size, f );
	fclose( f );

	void *pixels = qix_decode( data, bytes_read, desc, channels );
	QIX_FREE( data );
	return pixels;
}

#endif // QIX_NO_STDIO
#endif // QIX_IMPLEMENTATION
