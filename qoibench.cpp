/*

Simple benchmark suite for png, stbi and qoi

Requires libpng, "stb_image.h" and "stb_image_write.h"
Compile with: 
	gcc qoibench.c -std=gnu99 -lpng -O3 -o qoibench 

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

#include <stdio.h>
#include <png.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define QOI_IMPLEMENTATION
#include "qoi.h"

#include <filesystem>
#include <memory>
#include <vector>

// -----------------------------------------------------------------------------
// Cross platform high resolution timer
// From https://gist.github.com/ForeverZer0/0a4f80fc02b96e19380ebb7a3debbee5

#include <stdint.h>
#if defined(__linux)
	#define HAVE_POSIX_TIMER
	#include <time.h>
	#ifdef CLOCK_MONOTONIC
		#define CLOCKID CLOCK_MONOTONIC
	#else
		#define CLOCKID CLOCK_REALTIME
	#endif
#elif defined(__APPLE__)
	#define HAVE_MACH_TIMER
	#include <mach/mach_time.h>
#elif defined(_WIN32)
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#endif

static uint64_t ns() {
	static uint64_t is_init = 0;
#if defined(__APPLE__)
		static mach_timebase_info_data_t info;
		if (0 == is_init) {
			mach_timebase_info(&info);
			is_init = 1;
		}
		uint64_t now;
		now = mach_absolute_time();
		now *= info.numer;
		now /= info.denom;
		return now;
#elif defined(__linux)
		static struct timespec linux_rate;
		if (0 == is_init) {
			clock_getres(CLOCKID, &linux_rate);
			is_init = 1;
		}
		uint64_t now;
		struct timespec spec;
		clock_gettime(CLOCKID, &spec);
		now = spec.tv_sec * 1.0e9 + spec.tv_nsec;
		return now;
#elif defined(_WIN32)
		static LARGE_INTEGER win_frequency;
		if (0 == is_init) {
			QueryPerformanceFrequency(&win_frequency);
			is_init = 1;
		}
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		return (uint64_t) ((1e9 * now.QuadPart)	/ win_frequency.QuadPart);
#endif
}

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define QOI_ERROR(...) printf("abort at line " TOSTRING(__LINE__) ": " __VA_ARGS__); printf("\n"); exit(1)


// -----------------------------------------------------------------------------
// libpng encode/decode wrappers
// Seriously, who thought this was a good abstraction for an API to read/write
// images?

typedef struct {
	int size;
	int capacity;
	unsigned char *data;
} libpng_write_t;

void libpng_encode_callback(png_structp png_ptr, png_bytep data, png_size_t length) {
	libpng_write_t *write_data = (libpng_write_t*)png_get_io_ptr(png_ptr);
	if (write_data->size + length >= write_data->capacity) {
		QOI_ERROR("PNG write");
	}
	memcpy(write_data->data + write_data->size, data, length);
	write_data->size += (int)length;
}

void *libpng_encode(void *pixels, int w, int h, int *out_len) {
	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png) {
		QOI_ERROR("png_create_write_struct");
	}

	png_infop info = png_create_info_struct(png);
	if (!info) {
		QOI_ERROR("png_create_info_struct");
	}

	if (setjmp(png_jmpbuf(png))) {
		QOI_ERROR("png_jmpbuf");
	}

	// Output is 8bit depth, RGBA format.
	png_set_IHDR(
		png,
		info,
		w, h,
		8,
		PNG_COLOR_TYPE_RGBA,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT
	);

	auto row_pointers = std::make_unique<png_bytep[]>(h);
	for(int y = 0; y < h; y++){
		row_pointers[y] = ((unsigned char *)pixels + y * w * 4);
	}

	libpng_write_t write_data = {
		.size = 0,
		.capacity = w * h * 4,
		.data = (unsigned char *)malloc(w * h * 4)
	};

	png_set_rows(png, info, row_pointers.get());
	png_set_write_fn(png, &write_data, libpng_encode_callback, NULL);
	png_write_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);

	png_destroy_write_struct(&png, &info);

	*out_len = write_data.size;
	return write_data.data;
}


typedef struct {
	int pos;
	int size;
	unsigned char *data;
} libpng_read_t;

void png_decode_callback(png_structp png, png_bytep data, png_size_t length) {
	libpng_read_t *read_data = (libpng_read_t*)png_get_io_ptr(png);
	if (read_data->pos + length > read_data->size) {
		QOI_ERROR("PNG read %d bytes at pos %d (size: %d)", (int)length, read_data->pos, read_data->size);
	}
	memcpy(data, read_data->data + read_data->pos, length);
	read_data->pos += (int)length;
}

void *libpng_decode(void *data, int size, int *out_w, int *out_h) {	
	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png) {
		QOI_ERROR("png_create_read_struct");
	}

	png_infop info = png_create_info_struct(png);
	if (!info) {
		QOI_ERROR("png_create_info_struct");
	}

	libpng_read_t read_data = {
		.pos = 0,
		.size = size,
		.data = (unsigned char *)data
	};
	
	png_set_read_fn(png, &read_data, png_decode_callback);
	png_set_sig_bytes(png, 0);
	png_read_info(png, info);
	
	png_uint_32 w, h;
	int bitDepth, colorType, interlaceType;
	png_get_IHDR(png, info, &w, &h, &bitDepth, &colorType, &interlaceType, NULL, NULL);
	
	// 16 bit -> 8 bit
	png_set_strip_16(png);
	
	// 1, 2, 4 bit -> 8 bit
	if (bitDepth < 8) {
		png_set_packing(png);
	}

	if (colorType & PNG_COLOR_MASK_PALETTE) {
		png_set_expand(png);
	}
	
	if (!(colorType & PNG_COLOR_MASK_COLOR)) {
		png_set_gray_to_rgb(png);
	}

	// set paletted or RGB images with transparency to full alpha so we get RGBA
	if (png_get_valid(png, info, PNG_INFO_tRNS)) {
		png_set_tRNS_to_alpha(png);
	}
	
	// make sure every pixel has an alpha value
	if (!(colorType & PNG_COLOR_MASK_ALPHA)) {
		png_set_filler(png, 255, PNG_FILLER_AFTER);
	}
	
	png_read_update_info(png, info);

	unsigned char* out = (unsigned char *)malloc(w * h * 4);
	*out_w = w;
	*out_h = h;
	
	// png_uint_32 rowBytes = png_get_rowbytes(png, info);
	auto row_pointers = std::make_unique<png_bytep[]>(h);
	for (png_uint_32 row = 0; row < h; row++ ) {
		row_pointers[row] = (png_bytep)(out + (row * w * 4));
	}
	
	png_read_image(png, row_pointers.get());
	png_read_end(png, info);
	png_destroy_read_struct( &png, &info, NULL);
	
	return out;
}


// -----------------------------------------------------------------------------
// stb_image encode callback

void stbi_write_callback(void *context, void *data, int size) {
	int *encoded_size = (int *)context;
	*encoded_size += size;
	// In theory we'd need to do another malloc(), memcpy() and free() here to 
	// be fair to the other decode functions...
}


// -----------------------------------------------------------------------------
// function to load a whole file into memory

void *fload(const char *path, int *out_size) {
	FILE *fh = fopen(path, "rb");
	if (!fh) {
		QOI_ERROR("Can't open file");
	}

	fseek(fh, 0, SEEK_END);
	int size = ftell(fh);
	fseek(fh, 0, SEEK_SET);

	void *buffer = malloc(size);
	if (!buffer) {
		QOI_ERROR("Malloc for %d bytes failed", size);
	}

	if (!fread(buffer, size, 1, fh)) {
		QOI_ERROR("Can't read file %s", path);
	}
	fclose(fh);

	*out_size = size;
	return buffer;
}


// -----------------------------------------------------------------------------
// benchmark runner

typedef struct {
	uint64_t size;
	uint64_t encode_time;
	uint64_t decode_time;
} benchmark_lib_result_t;

typedef struct {
	stats_t stats;
	uint64_t px;
	int w;
	int h;
	benchmark_lib_result_t libpng;
	benchmark_lib_result_t stbi;
	benchmark_lib_result_t qoi;
} benchmark_result_t;

struct benchmark_conf {
	bool encode = true;
	bool decode = true;
	bool alphaToBW = false;
	bool saveQOI = false;
};

// Run __VA_ARGS__ a number of times and meassure the time taken. The first
// run is ignored.
#define BENCHMARK_FN(RUNS, AVG_TIME, ...) \
	do { \
		uint64_t time = 0; \
		for (int i = 0; i <= RUNS; i++) { \
			uint64_t time_start = ns(); \
			__VA_ARGS__ \
			uint64_t time_end = ns(); \
			if (i > 0) { \
				time += time_end - time_start; \
			} \
		} \
		AVG_TIME = time / RUNS; \
	} while (0)

benchmark_result_t benchmark_image(const char* path, int runs, benchmark_conf conf = { }) {
	int encoded_png_size;
	int encoded_qoi_size;
	int w;
	int h;

	// Load the encoded PNG, encoded QOI and raw pixels into memory
	void *pixels = (void *)stbi_load(path, &w, &h, NULL, 4);
	void *encoded_png = fload(path, &encoded_png_size);

	if (conf.alphaToBW)
	{
		unsigned char* px_ptr = (unsigned char*)pixels;
		for (int i = 0, S = w * h; i < S; ++i, px_ptr += 4) {
			px_ptr[0] = px_ptr[3];
			px_ptr[1] = px_ptr[3];
			px_ptr[2] = px_ptr[3];
			px_ptr[3] = 255;
		}
	}

	auto desc = qoi_desc{
		.width = (unsigned int)w,
		.height = (unsigned int)h,
		.channels = 4,
		.colorspace = QOI_SRGB,
		.mode = 1
	};

	benchmark_result_t res = { 0 };
	res.px = w * h;
	res.w = w;
	res.h = h;

	void *encoded_qoi = qoi_encode(pixels, &desc, &encoded_qoi_size, &res.stats);

	if (!pixels || !encoded_qoi || !encoded_png) {
		QOI_ERROR("Error decoding %s\n", path);
	}

	if (conf.saveQOI) {
		std::string fileName = path;
		fileName += "_encoded.qoi";

		FILE* f = fopen(fileName.c_str(), "wb");
		fwrite(encoded_qoi, 1, encoded_qoi_size, f);
		fclose(f);
	}

	// Decoding
	if (runs > 0) {
		BENCHMARK_FN(runs, res.libpng.decode_time, {
			int dec_w, dec_h;
			void* dec_p = libpng_decode(encoded_png, encoded_png_size, &dec_w, &dec_h);
			free(dec_p);
			});

		BENCHMARK_FN(runs, res.stbi.decode_time, {
			int dec_w, dec_h, dec_channels;
			void* dec_p = stbi_load_from_memory((const stbi_uc*)encoded_png, encoded_png_size, &dec_w, &dec_h, &dec_channels, 4);
			free(dec_p);
			});
	}

	if (conf.decode) {
		BENCHMARK_FN(abs(runs), res.qoi.decode_time, {
			qoi_desc desc;
			void* dec_p = qoi_decode(encoded_qoi, encoded_qoi_size, &desc, 4);
			free(dec_p);
			});
	}

	// Encoding
	if (runs > 0) {
		BENCHMARK_FN(runs, res.libpng.encode_time, {
			int enc_size;
			void* enc_p = libpng_encode(pixels, w, h, &enc_size);
			res.libpng.size = enc_size;
			free(enc_p);
			});

		BENCHMARK_FN(runs, res.stbi.encode_time, {
			int enc_size = 0;
			stbi_write_png_to_func(stbi_write_callback, &enc_size, w, h, 4, pixels, 0);
			res.stbi.size = enc_size;
			});
	}

	if (conf.encode) {
		BENCHMARK_FN(abs(runs), res.qoi.encode_time, {
			int enc_size;
			auto desc = qoi_desc{
				.width = (unsigned int)w,
				.height = (unsigned int)h,
				.channels = 4,
				.colorspace = QOI_SRGB,
				.mode = 1
			};
			void* enc_p = qoi_encode(pixels, &desc, &enc_size, NULL);
			res.qoi.size = enc_size;
			free(enc_p);
			});
	}

	free(pixels);
	free(encoded_png);
	free(encoded_qoi);

	return res;
}

void benchmark_print_header(const char *head) {
	char buff[256] = { };
	sprintf(buff, "%s", head);

	if (strlen(buff) >= 39)
		buff[39] = 0;

	printf("%s", buff);
	for (int i = 0, S = 39 - (int)strlen(buff); i < S; ++i) printf(" ");

	printf(
		"|   index    diff_8    diff_16    run_8    diff_24    color  | size kB\n");
}

void benchmark_print_separator() {
	printf(
		"---------------------------------------+------------------------------------------------------------+--------\n");
}

void benchmark_print_simple_result(const char* head, benchmark_result_t res) {
	char buff[256] = { };
	sprintf(buff, "%s", head);
	
	if (strlen(buff) >= 39)
		buff[39] = 0;

	printf("%s", buff);
	for (int i = 0, S = 39 - (int)strlen(buff); i < S; ++i) printf(" ");

	printf(
		"|%8d  %8d   %8d %8d   %8d %8d  |%8d\n",
		(int)res.stats.count_index,
		(int)res.stats.count_diff_8,
		(int)res.stats.count_diff_16,
		(int)res.stats.count_run_8,
		(int)res.stats.count_diff_24,
		(int)res.stats.count_color,
		(int)res.qoi.size / 1024
	);
}

void benchmark_print_result(const char *head, benchmark_result_t res, int runs) {
	double px = (double)res.px;
	printf("## %s size: %dx%d\n", head, res.w, res.h);
	printf(
		"-------------------------------------------------------------------\n");

	printf("        decode ms   encode ms   decode mpps   encode mpps   size kb\n");

	if (runs > 0) {
		printf(
			"libpng:  %8.1f    %8.1f      %8.2f      %8.2f  %8d\n",
			(double)res.libpng.decode_time / 1000000.0,
			(double)res.libpng.encode_time / 1000000.0,
			(res.libpng.decode_time > 0 ? px / ((double)res.libpng.decode_time / 1000.0) : 0),
			(res.libpng.encode_time > 0 ? px / ((double)res.libpng.encode_time / 1000.0) : 0),
			(int)res.libpng.size / 1024
		);
		printf(
			"stbi:    %8.1f    %8.1f      %8.2f      %8.2f  %8d\n",
			(double)res.stbi.decode_time / 1000000.0,
			(double)res.stbi.encode_time / 1000000.0,
			(res.stbi.decode_time > 0 ? px / ((double)res.stbi.decode_time / 1000.0) : 0),
			(res.stbi.encode_time > 0 ? px / ((double)res.stbi.encode_time / 1000.0) : 0),
			(int)res.stbi.size / 1024
		);
	}
	printf(
		"qoi:     %8.1f    %8.1f      %8.2f      %8.2f  %8d\n", 
		(double)res.qoi.decode_time/1000000.0,
		(double)res.qoi.encode_time/1000000.0,
		(res.qoi.decode_time > 0 ? px / ((double)res.qoi.decode_time/1000.0) : 0),
		(res.qoi.encode_time > 0 ? px / ((double)res.qoi.encode_time/1000.0) : 0),
		(int)res.qoi.size/1024
	);
	printf("\n");
}

int main(int argc, char **argv) {
	if (argc < 3) {
		printf("Usage: qoibench <iterations> <directory>\n");
		printf("Example: qoibench 10 images/textures/\n");
		exit(1);
	}

	float total_percentage = 0;
	int total_size = 0;

	int runs = atoi(argv[1]);
	namespace fs = std::filesystem;

	if (!fs::exists(argv[2])) {
		QOI_ERROR("Couldn't open directory %s", argv[2]);
	}

	struct dir_suite {
		std::string name;
		std::vector<std::string> files;
		benchmark_result_t totals = { 0 };

		void get_files(const fs::path& p) {
			name = p.filename().string();

			for (const auto& dir_entry : fs::directory_iterator{ p }) {
				if (dir_entry.is_directory() || dir_entry.path().extension() != ".png") {
					continue;
				}

				files.push_back(dir_entry.path().string());
			}
		}
	};
	
	std::vector<dir_suite> dir_suites;
	dir_suites.emplace_back();

	fs::path files_path = argv[2];
	if (fs::is_directory(files_path)) {
		for (const auto& dir_entry : fs::directory_iterator{ files_path }) {
			if (dir_entry.is_directory()) {
				auto& suite = dir_suites.emplace_back();
				suite.get_files(dir_entry.path());
			}
			else if (dir_entry.path().extension() == ".png") {
				dir_suites[0].files.push_back(dir_entry.path().string());
			}
		}
	}
	else {
		dir_suites[0].files.push_back(files_path.string());
	}

	benchmark_conf conf;
	conf.encode = true;
	conf.decode = false;
	conf.alphaToBW = true;
	conf.saveQOI = true;

	for (auto& suite : dir_suites) {
		if (suite.files.empty())
			continue;

		if (runs < 0) {
			benchmark_print_header(suite.name.c_str());
			benchmark_print_separator();
		}

		for (const auto& file_path : suite.files) {

			benchmark_result_t res = benchmark_image(file_path.c_str(), runs, conf);
			
			if (runs > 0)
				benchmark_print_result(file_path.c_str(), res, runs);
			else
				benchmark_print_simple_result(fs::path(file_path).filename().string().c_str(), res);

			suite.totals.px += res.px;
			suite.totals.libpng.encode_time += res.libpng.encode_time;
			suite.totals.libpng.decode_time += res.libpng.decode_time;
			suite.totals.libpng.size += res.libpng.size;
			suite.totals.stbi.encode_time += res.stbi.encode_time;
			suite.totals.stbi.decode_time += res.stbi.decode_time;
			suite.totals.stbi.size += res.stbi.size;
			suite.totals.qoi.encode_time += res.qoi.encode_time;
			suite.totals.qoi.decode_time += res.qoi.decode_time;
			suite.totals.qoi.size += res.qoi.size;
		}

		int count = int(suite.files.size());
		suite.totals.px /= count;
		suite.totals.libpng.encode_time /= count;
		suite.totals.libpng.decode_time /= count;
		suite.totals.libpng.size /= count;
		suite.totals.stbi.encode_time /= count;
		suite.totals.stbi.decode_time /= count;
		suite.totals.stbi.size /= count;
		suite.totals.qoi.encode_time /= count;
		suite.totals.qoi.decode_time /= count;
		suite.totals.qoi.size /= count;

		if (runs > 0) {
			benchmark_print_result("Total AVG", suite.totals, runs);
		}
		else {
			benchmark_print_separator();
			benchmark_print_simple_result(suite.name.c_str(), suite.totals);
		}

		printf("\n");
	}

	if (dir_suites.size() > 1) {
		benchmark_print_header("");
		benchmark_print_separator();

		for (const auto& suite : dir_suites) {
			if (suite.files.empty())
				continue;

			benchmark_print_simple_result(suite.name.c_str(), suite.totals);
		}
	}
	else {
		if (runs <= 0)
			benchmark_print_result("Total AVG", dir_suites[0].totals, 1);
	}

	return 0;
}
