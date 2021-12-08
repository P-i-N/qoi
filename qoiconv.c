/*

Command line tool to convert between png <> qoi format

Requires "stb_image.h" and "stb_image_write.h"
Compile with:
	gcc qoiconv.c -std=c99 -O3 -o qoiconv

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

*/


#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define QIX_IMPLEMENTATION
#include "qix.h"


#define STR_ENDS_WITH(S, E) (strcmp(S + strlen(S) - (sizeof(E)-1), E) == 0)

int main( int argc, char **argv )
{
	if ( argc < 3 )
	{
		printf( "Usage: qoiconv <infile> <outfile>\n" );
		printf( "Examples:\n" );
		printf( "  qoiconv input.png output.qoi\n" );
		printf( "  qoiconv input.qoi output.png\n" );
		exit( 1 );
	}

	void *pixels = NULL;
	int w, h, channels;

	printf( "Reading %s\n", argv[1] );

	if ( STR_ENDS_WITH( argv[1], ".png" ) )
	{
		pixels = ( void * )stbi_load( argv[1], &w, &h, &channels, 0 );
	}
	else if ( STR_ENDS_WITH( argv[1], ".qix" ) )
	{
		qix_desc desc;
		pixels = qix_read( argv[1], &desc, 0 );
		channels = desc.channels;
		w = desc.width;
		h = desc.height;
	}

	if ( pixels == NULL )
	{
		printf( "Couldn't load/decode %s\n", argv[1] );
		exit( 1 );
	}

	printf( "Writing %s\n", argv[2] );

	int encoded = 0;
	if ( STR_ENDS_WITH( argv[2], ".png" ) )
	{
		encoded = stbi_write_png( argv[2], w, h, channels, pixels, 0 );
	}
	else if ( STR_ENDS_WITH( argv[2], ".qix" ) )
	{
		encoded = qix_write( argv[2], pixels, &( qix_desc )
		{
			.width = w,
			.height = h,
			.channels = channels,
			.colorspace = QIX_SRGB
		} );

		// Try decoding as well...
		printf( "Reading %s\n", argv[2] );

		qix_desc desc;
		pixels = qix_read( argv[2], &desc, 0 );
		channels = desc.channels;
		w = desc.width;
		h = desc.height;

		if ( pixels )
		{
			char buff[256] = { 0 };
			sprintf( buff, "%s_decoded.png", argv[2] );
			stbi_write_png( buff, w, h, channels, pixels, 0 );
		}
	}

	if ( !encoded )
	{
		printf( "Couldn't write/encode %s\n", argv[2] );
		exit( 1 );
	}

	free( pixels );
	return 0;
}
