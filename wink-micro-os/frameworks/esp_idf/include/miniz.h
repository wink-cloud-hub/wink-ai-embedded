/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef MINIZ_HEADER_INCLUDED
#define MINIZ_HEADER_INCLUDED
#ifndef __WINK_HARVESTED_MINIZ_H__
#define __WINK_HARVESTED_MINIZ_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdlib.h>
#include <time.h>


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef MAX_MEM_LEVEL
#define MAX_MEM_LEVEL 9
#endif
#ifndef MAX_WBITS
#define MAX_WBITS 15
#endif
#ifndef MINIZ_HAS_64BIT_REGISTERS
#define MINIZ_HAS_64BIT_REGISTERS 0
#endif
#ifndef MINIZ_LITTLE_ENDIAN
#define MINIZ_LITTLE_ENDIAN 1
#endif
#ifndef MINIZ_USE_UNALIGNED_LOADS_AND_STORES
#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 0
#endif
#ifndef MINIZ_X86_OR_X64_CPU
#define MINIZ_X86_OR_X64_CPU 0
#endif
#ifndef MZ_ADLER32_INIT
#define MZ_ADLER32_INIT (1)
#endif
#ifndef MZ_CRC32_INIT
#define MZ_CRC32_INIT (0)
#endif
#ifndef MZ_DEFAULT_WINDOW_BITS
#define MZ_DEFAULT_WINDOW_BITS 15
#endif
#ifndef MZ_DEFLATED
#define MZ_DEFLATED 8
#endif
#ifndef MZ_FALSE
#define MZ_FALSE (0)
#endif
#ifndef MZ_MACRO_END
#define MZ_MACRO_END while (0, 0)
#endif
#ifndef MZ_TRUE
#define MZ_TRUE (1)
#endif
#ifndef MZ_VERNUM
#define MZ_VERNUM 0x91F0
#endif
#ifndef MZ_VERSION
#define MZ_VERSION "9.1.15"
#endif
#ifndef MZ_VER_MAJOR
#define MZ_VER_MAJOR 9
#endif
#ifndef MZ_VER_MINOR
#define MZ_VER_MINOR 1
#endif
#ifndef MZ_VER_REVISION
#define MZ_VER_REVISION 15
#endif
#ifndef MZ_VER_SUBREVISION
#define MZ_VER_SUBREVISION 0
#endif
#ifndef TDEFL_LESS_MEMORY
#define TDEFL_LESS_MEMORY 1
#endif
#ifndef TINFL_BITBUF_SIZE
#define TINFL_BITBUF_SIZE (64)
#endif
#ifndef TINFL_DECOMPRESS_MEM_TO_MEM_FAILED
#define TINFL_DECOMPRESS_MEM_TO_MEM_FAILED ((size_t)(-1))
#endif
#ifndef TINFL_LZ_DICT_SIZE
#define TINFL_LZ_DICT_SIZE 32768
#endif
#ifndef TINFL_USE_64BIT_BITBUF
#define TINFL_USE_64BIT_BITBUF 0
#endif
#ifndef ZLIB_VERNUM
#define ZLIB_VERNUM MZ_VERNUM
#endif
#ifndef ZLIB_VERSION
#define ZLIB_VERSION MZ_VERSION
#endif
#ifndef ZLIB_VER_MAJOR
#define ZLIB_VER_MAJOR MZ_VER_MAJOR
#endif
#ifndef ZLIB_VER_MINOR
#define ZLIB_VER_MINOR MZ_VER_MINOR
#endif
#ifndef ZLIB_VER_REVISION
#define ZLIB_VER_REVISION MZ_VER_REVISION
#endif
#ifndef ZLIB_VER_SUBREVISION
#define ZLIB_VER_SUBREVISION MZ_VER_SUBREVISION
#endif
#ifndef Z_BEST_COMPRESSION
#define Z_BEST_COMPRESSION MZ_BEST_COMPRESSION
#endif
#ifndef Z_BEST_SPEED
#define Z_BEST_SPEED MZ_BEST_SPEED
#endif
#ifndef Z_BLOCK
#define Z_BLOCK MZ_BLOCK
#endif
#ifndef Z_BUF_ERROR
#define Z_BUF_ERROR MZ_BUF_ERROR
#endif
#ifndef Z_DATA_ERROR
#define Z_DATA_ERROR MZ_DATA_ERROR
#endif
#ifndef Z_DEFAULT_COMPRESSION
#define Z_DEFAULT_COMPRESSION MZ_DEFAULT_COMPRESSION
#endif
#ifndef Z_DEFAULT_STRATEGY
#define Z_DEFAULT_STRATEGY MZ_DEFAULT_STRATEGY
#endif
#ifndef Z_DEFAULT_WINDOW_BITS
#define Z_DEFAULT_WINDOW_BITS MZ_DEFAULT_WINDOW_BITS
#endif
#ifndef Z_DEFLATED
#define Z_DEFLATED MZ_DEFLATED
#endif
#ifndef Z_ERRNO
#define Z_ERRNO MZ_ERRNO
#endif
#ifndef Z_FILTERED
#define Z_FILTERED MZ_FILTERED
#endif
#ifndef Z_FINISH
#define Z_FINISH MZ_FINISH
#endif
#ifndef Z_FIXED
#define Z_FIXED MZ_FIXED
#endif
#ifndef Z_FULL_FLUSH
#define Z_FULL_FLUSH MZ_FULL_FLUSH
#endif
#ifndef Z_HUFFMAN_ONLY
#define Z_HUFFMAN_ONLY MZ_HUFFMAN_ONLY
#endif
#ifndef Z_MEM_ERROR
#define Z_MEM_ERROR MZ_MEM_ERROR
#endif
#ifndef Z_NEED_DICT
#define Z_NEED_DICT MZ_NEED_DICT
#endif
#ifndef Z_NO_COMPRESSION
#define Z_NO_COMPRESSION MZ_NO_COMPRESSION
#endif
#ifndef Z_NO_FLUSH
#define Z_NO_FLUSH MZ_NO_FLUSH
#endif
#ifndef Z_NULL
#define Z_NULL 0
#endif
#ifndef Z_OK
#define Z_OK MZ_OK
#endif
#ifndef Z_PARAM_ERROR
#define Z_PARAM_ERROR MZ_PARAM_ERROR
#endif
#ifndef Z_PARTIAL_FLUSH
#define Z_PARTIAL_FLUSH MZ_PARTIAL_FLUSH
#endif
#ifndef Z_RLE
#define Z_RLE MZ_RLE
#endif
#ifndef Z_STREAM_END
#define Z_STREAM_END MZ_STREAM_END
#endif
#ifndef Z_STREAM_ERROR
#define Z_STREAM_ERROR MZ_STREAM_ERROR
#endif
#ifndef Z_SYNC_FLUSH
#define Z_SYNC_FLUSH MZ_SYNC_FLUSH
#endif
#ifndef Z_VERSION_ERROR
#define Z_VERSION_ERROR MZ_VERSION_ERROR
#endif
#ifndef adler32
#define adler32 mz_adler32
#endif
#ifndef alloc_func
#define alloc_func mz_alloc_func
#endif
#ifndef compress
#define compress mz_compress
#endif
#ifndef compress2
#define compress2 mz_compress2
#endif
#ifndef compressBound
#define compressBound mz_compressBound
#endif
#ifndef crc32
#define crc32 mz_crc32
#endif
#ifndef deflate
#define deflate mz_deflate
#endif
#ifndef deflateBound
#define deflateBound mz_deflateBound
#endif
#ifndef deflateEnd
#define deflateEnd mz_deflateEnd
#endif
#ifndef deflateInit
#define deflateInit mz_deflateInit
#endif
#ifndef deflateInit2
#define deflateInit2 mz_deflateInit2
#endif
#ifndef deflateReset
#define deflateReset mz_deflateReset
#endif
#ifndef free_func
#define free_func mz_free_func
#endif
#ifndef inflate
#define inflate mz_inflate
#endif
#ifndef inflateEnd
#define inflateEnd mz_inflateEnd
#endif
#ifndef inflateInit
#define inflateInit mz_inflateInit
#endif
#ifndef inflateInit2
#define inflateInit2 mz_inflateInit2
#endif
#ifndef internal_state
#define internal_state mz_internal_state
#endif
#ifndef mz_crc32
#define mz_crc32 esp_rom_crc32_le
#endif
#ifndef tinfl_get_adler32
#define tinfl_get_adler32(r) (r)->m_check_adler32
#endif
#ifndef tinfl_init
#define tinfl_init(r) do { (r)->m_state = 0; } MZ_MACRO_END
#endif
#ifndef uncompress
#define uncompress mz_uncompress
#endif
#ifndef zError
#define zError mz_error
#endif
#ifndef z_stream
#define z_stream mz_stream
#endif
#ifndef zlibVersion
#define zlibVersion mz_version
#endif
#ifndef zlib_version
#define zlib_version mz_version()
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    MZ_ZIP_MODE_INVALID = 0,
    MZ_ZIP_MODE_READING = 1,
    MZ_ZIP_MODE_WRITING = 2,
    MZ_ZIP_MODE_WRITING_HAS_BEEN_FINALIZED = 3,
} mz_zip_mode;
typedef enum {
    MZ_ZIP_FLAG_CASE_SENSITIVE = 0x0100,
    MZ_ZIP_FLAG_IGNORE_PATH = 0x0200,
    MZ_ZIP_FLAG_COMPRESSED_DATA = 0x0400,
    MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY = 0x0800,
} mz_zip_flags;
typedef enum {
    TINFL_STATUS_BAD_PARAM = -3,
    TINFL_STATUS_ADLER32_MISMATCH = -2,
    TINFL_STATUS_FAILED = -1,
    TINFL_STATUS_DONE = 0,
    TINFL_STATUS_NEEDS_MORE_INPUT = 1,
    TINFL_STATUS_HAS_MORE_OUTPUT = 2,
} tinfl_status;
typedef enum {
    TDEFL_STATUS_BAD_PARAM = -2,
    TDEFL_STATUS_PUT_BUF_FAILED = -1,
    TDEFL_STATUS_OKAY = 0,
    TDEFL_STATUS_DONE = 1,
} tdefl_status;
typedef enum {
    TDEFL_NO_FLUSH = 0,
    TDEFL_SYNC_FLUSH = 2,
    TDEFL_FULL_FLUSH = 3,
    TDEFL_FINISH = 4,
} tdefl_flush;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef unsigned long mz_ulong;
typedef void *(*mz_alloc_func)(void *opaque, size_t items, size_t size);
typedef void (*mz_free_func)(void *opaque, void *address);
typedef void *(*mz_realloc_func)(void *opaque, void *address, size_t items, size_t size);
typedef struct {
    const unsigned char * next_in;
    unsigned int avail_in;
    mz_ulong total_in;
    unsigned char * next_out;
    unsigned int avail_out;
    mz_ulong total_out;
    char * msg;
    struct mz_internal_state * state;
    mz_alloc_func zalloc;
    mz_free_func zfree;
    void * opaque;
    int data_type;
    mz_ulong adler;
    mz_ulong reserved;
} mz_stream;
typedef mz_stream * mz_streamp;
typedef unsigned char Byte;
typedef unsigned int uInt;
typedef mz_ulong uLong;
typedef Byte Bytef;
typedef uInt uIntf;
typedef char charf;
typedef int intf;
typedef void * voidpf;
typedef uLong uLongf;
typedef void * voidp;
typedef void const * voidpc;
typedef unsigned char mz_uint8;
typedef signed short mz_int16;
typedef unsigned short mz_uint16;
typedef unsigned int mz_uint32;
typedef unsigned int mz_uint;
typedef long long mz_int64;
typedef unsigned long long mz_uint64;
typedef int mz_bool;
typedef struct {
    mz_uint32 m_file_index;
    mz_uint32 m_central_dir_ofs;
    mz_uint16 m_version_made_by;
    mz_uint16 m_version_needed;
    mz_uint16 m_bit_flag;
    mz_uint16 m_method;
    time_t m_time;
    mz_uint32 m_crc32;
    mz_uint64 m_comp_size;
    mz_uint64 m_uncomp_size;
    mz_uint16 m_internal_attr;
    mz_uint32 m_external_attr;
    mz_uint64 m_local_header_ofs;
    mz_uint32 m_comment_size;
    char m_filename[MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE];
    char m_comment[MZ_ZIP_MAX_ARCHIVE_FILE_COMMENT_SIZE];
} mz_zip_archive_file_stat;
typedef size_t (*mz_file_read_func)(void *pOpaque, mz_uint64 file_ofs, void *pBuf, size_t n);
typedef size_t (*mz_file_write_func)(void *pOpaque, mz_uint64 file_ofs, const void *pBuf, size_t n);
typedef struct mz_zip_internal_state_tag mz_zip_internal_state;
typedef struct {
    mz_uint64 m_archive_size;
    mz_uint64 m_central_directory_file_ofs;
    mz_uint m_total_files;
    mz_zip_mode m_zip_mode;
    mz_uint m_file_offset_alignment;
    mz_alloc_func m_pAlloc;
    mz_free_func m_pFree;
    mz_realloc_func m_pRealloc;
    void * m_pAlloc_opaque;
    mz_file_read_func m_pRead;
    mz_file_write_func m_pWrite;
    void * m_pIO_opaque;
    mz_zip_internal_state * m_pState;
} mz_zip_archive;
typedef int (*tinfl_put_buf_func_ptr)(const void* pBuf, int len, void *pUser);
typedef struct {
    mz_uint8 m_code_size[TINFL_MAX_HUFF_SYMBOLS_0];
    mz_int16 m_look_up[TINFL_FAST_LOOKUP_SIZE], m_tree[TINFL_MAX_HUFF_SYMBOLS_0*2];
} tinfl_huff_table;
typedef mz_uint64 tinfl_bit_buf_t;
struct tinfl_decompressor_tag {
    mz_uint32 m_state, m_num_bits, m_zhdr0, m_zhdr1, m_z_adler32, m_final, m_type, m_check_adler32, m_dist, m_counter, m_num_extra, m_table_sizes[TINFL_MAX_HUFF_TABLES];
    tinfl_bit_buf_t m_bit_buf;
    size_t m_dist_from_out_buf_start;
    tinfl_huff_table m_tables[TINFL_MAX_HUFF_TABLES];
    mz_uint8 m_raw_header[4], m_len_codes[TINFL_MAX_HUFF_SYMBOLS_0+TINFL_MAX_HUFF_SYMBOLS_1+137];
};
typedef mz_bool (*tdefl_put_buf_func_ptr)(const void *pBuf, int len, void *pUser);
typedef struct {
    tdefl_put_buf_func_ptr m_pPut_buf_func;
    void * m_pPut_buf_user;
    mz_uint m_flags, m_max_probes[2];
    int m_greedy_parsing;
    mz_uint m_adler32, m_lookahead_pos, m_lookahead_size, m_dict_size;
    mz_uint8 *m_pLZ_code_buf, *m_pLZ_flags, *m_pOutput_buf, * m_pOutput_buf_end;
    mz_uint m_num_flags_left, m_total_lz_bytes, m_lz_code_buf_dict_pos, m_bits_in, m_bit_buffer;
    mz_uint m_saved_match_dist, m_saved_match_len, m_saved_lit, m_output_flush_ofs, m_output_flush_remaining, m_finished, m_block_index, m_wants_to_finish;
    tdefl_status m_prev_return_status;
    const void * m_pIn_buf;
    void * m_pOut_buf;
    size_t *m_pIn_buf_size, * m_pOut_buf_size;
    tdefl_flush m_flush;
    const mz_uint8 * m_pSrc;
    size_t m_src_buf_left, m_out_buf_ofs;
    mz_uint8 m_dict[TDEFL_LZ_DICT_SIZE+TDEFL_MAX_MATCH_LEN-1];
    mz_uint16 m_huff_count[TDEFL_MAX_HUFF_TABLES][TDEFL_MAX_HUFF_SYMBOLS];
    mz_uint16 m_huff_codes[TDEFL_MAX_HUFF_TABLES][TDEFL_MAX_HUFF_SYMBOLS];
    mz_uint8 m_huff_code_sizes[TDEFL_MAX_HUFF_TABLES][TDEFL_MAX_HUFF_SYMBOLS];
    mz_uint8 m_lz_code_buf[TDEFL_LZ_CODE_BUF_SIZE];
    mz_uint16 m_next[TDEFL_LZ_DICT_SIZE];
    mz_uint16 m_hash[TDEFL_LZ_HASH_SIZE];
    mz_uint8 m_output_buf[TDEFL_OUT_BUF_SIZE];
} tdefl_compressor;



#if defined(__WINK_SIM__)
typedef int(*tinfl_put_buf_func_ptr)(const void* pBuf, int len, void *pUser) WINK_SLA_ERROR("Wink SLA Violation: int out of Core 8 scope.");
#else
typedef int(*tinfl_put_buf_func_ptr)(const void* pBuf, int len, void *pUser);
#endif

#if defined(__WINK_SIM__)
mz_ulong mz_adler32(mz_ulong adler, const unsigned char *ptr, size_t buf_len) WINK_SLA_ERROR("Wink SLA Violation: mz_adler32 out of Core 8 scope.");
#else
mz_ulong mz_adler32(mz_ulong adler, const unsigned char *ptr, size_t buf_len);
#endif

#if defined(__WINK_SIM__)
typedef mz_bool(*tdefl_put_buf_func_ptr)(const void *pBuf, int len, void *pUser) WINK_SLA_ERROR("Wink SLA Violation: mz_bool out of Core 8 scope.");
#else
typedef mz_bool(*tdefl_put_buf_func_ptr)(const void *pBuf, int len, void *pUser);
#endif

#if defined(__WINK_SIM__)
int mz_compress(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len) WINK_SLA_ERROR("Wink SLA Violation: mz_compress out of Core 8 scope.");
#else
int mz_compress(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len);
#endif

#if defined(__WINK_SIM__)
int mz_compress2(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len, int level) WINK_SLA_ERROR("Wink SLA Violation: mz_compress2 out of Core 8 scope.");
#else
int mz_compress2(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len, int level);
#endif

#if defined(__WINK_SIM__)
mz_ulong mz_compressBound(mz_ulong source_len) WINK_SLA_ERROR("Wink SLA Violation: mz_compressBound out of Core 8 scope.");
#else
mz_ulong mz_compressBound(mz_ulong source_len);
#endif

#if defined(__WINK_SIM__)
mz_ulong mz_crc32(mz_ulong crc, const unsigned char *ptr, size_t buf_len) WINK_SLA_ERROR("Wink SLA Violation: mz_crc32 out of Core 8 scope.");
#else
mz_ulong mz_crc32(mz_ulong crc, const unsigned char *ptr, size_t buf_len);
#endif

#if defined(__WINK_SIM__)
int mz_deflate(mz_streamp pStream, int flush) WINK_SLA_ERROR("Wink SLA Violation: mz_deflate out of Core 8 scope.");
#else
int mz_deflate(mz_streamp pStream, int flush);
#endif

#if defined(__WINK_SIM__)
mz_ulong mz_deflateBound(mz_streamp pStream, mz_ulong source_len) WINK_SLA_ERROR("Wink SLA Violation: mz_deflateBound out of Core 8 scope.");
#else
mz_ulong mz_deflateBound(mz_streamp pStream, mz_ulong source_len);
#endif

#if defined(__WINK_SIM__)
int mz_deflateEnd(mz_streamp pStream) WINK_SLA_ERROR("Wink SLA Violation: mz_deflateEnd out of Core 8 scope.");
#else
int mz_deflateEnd(mz_streamp pStream);
#endif

#if defined(__WINK_SIM__)
int mz_deflateInit(mz_streamp pStream, int level) WINK_SLA_ERROR("Wink SLA Violation: mz_deflateInit out of Core 8 scope.");
#else
int mz_deflateInit(mz_streamp pStream, int level);
#endif

#if defined(__WINK_SIM__)
int mz_deflateInit2(mz_streamp pStream, int level, int method, int window_bits, int mem_level, int strategy) WINK_SLA_ERROR("Wink SLA Violation: mz_deflateInit2 out of Core 8 scope.");
#else
int mz_deflateInit2(mz_streamp pStream, int level, int method, int window_bits, int mem_level, int strategy);
#endif

#if defined(__WINK_SIM__)
int mz_deflateReset(mz_streamp pStream) WINK_SLA_ERROR("Wink SLA Violation: mz_deflateReset out of Core 8 scope.");
#else
int mz_deflateReset(mz_streamp pStream);
#endif

#if defined(__WINK_SIM__)
const char * mz_error(int err) WINK_SLA_ERROR("Wink SLA Violation: mz_error out of Core 8 scope.");
#else
const char * mz_error(int err);
#endif

#if defined(__WINK_SIM__)
void mz_free(void *p) WINK_SLA_ERROR("Wink SLA Violation: mz_free out of Core 8 scope.");
#else
void mz_free(void *p);
#endif

#if defined(__WINK_SIM__)
int mz_inflate(mz_streamp pStream, int flush) WINK_SLA_ERROR("Wink SLA Violation: mz_inflate out of Core 8 scope.");
#else
int mz_inflate(mz_streamp pStream, int flush);
#endif

#if defined(__WINK_SIM__)
int mz_inflateEnd(mz_streamp pStream) WINK_SLA_ERROR("Wink SLA Violation: mz_inflateEnd out of Core 8 scope.");
#else
int mz_inflateEnd(mz_streamp pStream);
#endif

#if defined(__WINK_SIM__)
int mz_inflateInit(mz_streamp pStream) WINK_SLA_ERROR("Wink SLA Violation: mz_inflateInit out of Core 8 scope.");
#else
int mz_inflateInit(mz_streamp pStream);
#endif

#if defined(__WINK_SIM__)
int mz_inflateInit2(mz_streamp pStream, int window_bits) WINK_SLA_ERROR("Wink SLA Violation: mz_inflateInit2 out of Core 8 scope.");
#else
int mz_inflateInit2(mz_streamp pStream, int window_bits);
#endif

#if defined(__WINK_SIM__)
int mz_uncompress(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len) WINK_SLA_ERROR("Wink SLA Violation: mz_uncompress out of Core 8 scope.");
#else
int mz_uncompress(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len);
#endif

#if defined(__WINK_SIM__)
const char * mz_version(void) WINK_SLA_ERROR("Wink SLA Violation: mz_version out of Core 8 scope.");
#else
const char * mz_version(void);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_add_mem_to_archive_file_in_place(const char *pZip_filename, const char *pArchive_name, const void *pBuf, size_t buf_size, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_add_mem_to_archive_file_in_place out of Core 8 scope.");
#else
mz_bool mz_zip_add_mem_to_archive_file_in_place(const char *pZip_filename, const char *pArchive_name, const void *pBuf, size_t buf_size, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags);
#endif

#if defined(__WINK_SIM__)
void * mz_zip_extract_archive_file_to_heap(const char *pZip_filename, const char *pArchive_name, size_t *pSize, mz_uint zip_flags) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_extract_archive_file_to_heap out of Core 8 scope.");
#else
void * mz_zip_extract_archive_file_to_heap(const char *pZip_filename, const char *pArchive_name, size_t *pSize, mz_uint zip_flags);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_reader_end(mz_zip_archive *pZip) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_reader_end out of Core 8 scope.");
#else
mz_bool mz_zip_reader_end(mz_zip_archive *pZip);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_reader_extract_file_to_callback(mz_zip_archive *pZip, const char *pFilename, mz_file_write_func pCallback, void *pOpaque, mz_uint flags) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_reader_extract_file_to_callback out of Core 8 scope.");
#else
mz_bool mz_zip_reader_extract_file_to_callback(mz_zip_archive *pZip, const char *pFilename, mz_file_write_func pCallback, void *pOpaque, mz_uint flags);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_reader_extract_file_to_file(mz_zip_archive *pZip, const char *pArchive_filename, const char *pDst_filename, mz_uint flags) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_reader_extract_file_to_file out of Core 8 scope.");
#else
mz_bool mz_zip_reader_extract_file_to_file(mz_zip_archive *pZip, const char *pArchive_filename, const char *pDst_filename, mz_uint flags);
#endif

#if defined(__WINK_SIM__)
void * mz_zip_reader_extract_file_to_heap(mz_zip_archive *pZip, const char *pFilename, size_t *pSize, mz_uint flags) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_reader_extract_file_to_heap out of Core 8 scope.");
#else
void * mz_zip_reader_extract_file_to_heap(mz_zip_archive *pZip, const char *pFilename, size_t *pSize, mz_uint flags);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_reader_extract_file_to_mem(mz_zip_archive *pZip, const char *pFilename, void *pBuf, size_t buf_size, mz_uint flags) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_reader_extract_file_to_mem out of Core 8 scope.");
#else
mz_bool mz_zip_reader_extract_file_to_mem(mz_zip_archive *pZip, const char *pFilename, void *pBuf, size_t buf_size, mz_uint flags);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_reader_extract_file_to_mem_no_alloc(mz_zip_archive *pZip, const char *pFilename, void *pBuf, size_t buf_size, mz_uint flags, void *pUser_read_buf, size_t user_read_buf_size) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_reader_extract_file_to_mem_no_alloc out of Core 8 scope.");
#else
mz_bool mz_zip_reader_extract_file_to_mem_no_alloc(mz_zip_archive *pZip, const char *pFilename, void *pBuf, size_t buf_size, mz_uint flags, void *pUser_read_buf, size_t user_read_buf_size);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_reader_extract_to_callback(mz_zip_archive *pZip, mz_uint file_index, mz_file_write_func pCallback, void *pOpaque, mz_uint flags) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_reader_extract_to_callback out of Core 8 scope.");
#else
mz_bool mz_zip_reader_extract_to_callback(mz_zip_archive *pZip, mz_uint file_index, mz_file_write_func pCallback, void *pOpaque, mz_uint flags);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_reader_extract_to_file(mz_zip_archive *pZip, mz_uint file_index, const char *pDst_filename, mz_uint flags) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_reader_extract_to_file out of Core 8 scope.");
#else
mz_bool mz_zip_reader_extract_to_file(mz_zip_archive *pZip, mz_uint file_index, const char *pDst_filename, mz_uint flags);
#endif

#if defined(__WINK_SIM__)
void * mz_zip_reader_extract_to_heap(mz_zip_archive *pZip, mz_uint file_index, size_t *pSize, mz_uint flags) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_reader_extract_to_heap out of Core 8 scope.");
#else
void * mz_zip_reader_extract_to_heap(mz_zip_archive *pZip, mz_uint file_index, size_t *pSize, mz_uint flags);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_reader_extract_to_mem(mz_zip_archive *pZip, mz_uint file_index, void *pBuf, size_t buf_size, mz_uint flags) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_reader_extract_to_mem out of Core 8 scope.");
#else
mz_bool mz_zip_reader_extract_to_mem(mz_zip_archive *pZip, mz_uint file_index, void *pBuf, size_t buf_size, mz_uint flags);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_reader_extract_to_mem_no_alloc(mz_zip_archive *pZip, mz_uint file_index, void *pBuf, size_t buf_size, mz_uint flags, void *pUser_read_buf, size_t user_read_buf_size) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_reader_extract_to_mem_no_alloc out of Core 8 scope.");
#else
mz_bool mz_zip_reader_extract_to_mem_no_alloc(mz_zip_archive *pZip, mz_uint file_index, void *pBuf, size_t buf_size, mz_uint flags, void *pUser_read_buf, size_t user_read_buf_size);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_reader_file_stat(mz_zip_archive *pZip, mz_uint file_index, mz_zip_archive_file_stat *pStat) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_reader_file_stat out of Core 8 scope.");
#else
mz_bool mz_zip_reader_file_stat(mz_zip_archive *pZip, mz_uint file_index, mz_zip_archive_file_stat *pStat);
#endif

#if defined(__WINK_SIM__)
mz_uint mz_zip_reader_get_filename(mz_zip_archive *pZip, mz_uint file_index, char *pFilename, mz_uint filename_buf_size) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_reader_get_filename out of Core 8 scope.");
#else
mz_uint mz_zip_reader_get_filename(mz_zip_archive *pZip, mz_uint file_index, char *pFilename, mz_uint filename_buf_size);
#endif

#if defined(__WINK_SIM__)
mz_uint mz_zip_reader_get_num_files(mz_zip_archive *pZip) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_reader_get_num_files out of Core 8 scope.");
#else
mz_uint mz_zip_reader_get_num_files(mz_zip_archive *pZip);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_reader_init(mz_zip_archive *pZip, mz_uint64 size, mz_uint32 flags) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_reader_init out of Core 8 scope.");
#else
mz_bool mz_zip_reader_init(mz_zip_archive *pZip, mz_uint64 size, mz_uint32 flags);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_reader_init_file(mz_zip_archive *pZip, const char *pFilename, mz_uint32 flags) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_reader_init_file out of Core 8 scope.");
#else
mz_bool mz_zip_reader_init_file(mz_zip_archive *pZip, const char *pFilename, mz_uint32 flags);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_reader_init_mem(mz_zip_archive *pZip, const void *pMem, size_t size, mz_uint32 flags) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_reader_init_mem out of Core 8 scope.");
#else
mz_bool mz_zip_reader_init_mem(mz_zip_archive *pZip, const void *pMem, size_t size, mz_uint32 flags);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_reader_is_file_a_directory(mz_zip_archive *pZip, mz_uint file_index) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_reader_is_file_a_directory out of Core 8 scope.");
#else
mz_bool mz_zip_reader_is_file_a_directory(mz_zip_archive *pZip, mz_uint file_index);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_reader_is_file_encrypted(mz_zip_archive *pZip, mz_uint file_index) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_reader_is_file_encrypted out of Core 8 scope.");
#else
mz_bool mz_zip_reader_is_file_encrypted(mz_zip_archive *pZip, mz_uint file_index);
#endif

#if defined(__WINK_SIM__)
int mz_zip_reader_locate_file(mz_zip_archive *pZip, const char *pName, const char *pComment, mz_uint flags) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_reader_locate_file out of Core 8 scope.");
#else
int mz_zip_reader_locate_file(mz_zip_archive *pZip, const char *pName, const char *pComment, mz_uint flags);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_writer_add_file(mz_zip_archive *pZip, const char *pArchive_name, const char *pSrc_filename, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_writer_add_file out of Core 8 scope.");
#else
mz_bool mz_zip_writer_add_file(mz_zip_archive *pZip, const char *pArchive_name, const char *pSrc_filename, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_writer_add_from_zip_reader(mz_zip_archive *pZip, mz_zip_archive *pSource_zip, mz_uint file_index) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_writer_add_from_zip_reader out of Core 8 scope.");
#else
mz_bool mz_zip_writer_add_from_zip_reader(mz_zip_archive *pZip, mz_zip_archive *pSource_zip, mz_uint file_index);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_writer_add_mem(mz_zip_archive *pZip, const char *pArchive_name, const void *pBuf, size_t buf_size, mz_uint level_and_flags) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_writer_add_mem out of Core 8 scope.");
#else
mz_bool mz_zip_writer_add_mem(mz_zip_archive *pZip, const char *pArchive_name, const void *pBuf, size_t buf_size, mz_uint level_and_flags);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_writer_add_mem_ex(mz_zip_archive *pZip, const char *pArchive_name, const void *pBuf, size_t buf_size, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags, mz_uint64 uncomp_size, mz_uint32 uncomp_crc32) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_writer_add_mem_ex out of Core 8 scope.");
#else
mz_bool mz_zip_writer_add_mem_ex(mz_zip_archive *pZip, const char *pArchive_name, const void *pBuf, size_t buf_size, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags, mz_uint64 uncomp_size, mz_uint32 uncomp_crc32);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_writer_end(mz_zip_archive *pZip) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_writer_end out of Core 8 scope.");
#else
mz_bool mz_zip_writer_end(mz_zip_archive *pZip);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_writer_finalize_archive(mz_zip_archive *pZip) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_writer_finalize_archive out of Core 8 scope.");
#else
mz_bool mz_zip_writer_finalize_archive(mz_zip_archive *pZip);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_writer_finalize_heap_archive(mz_zip_archive *pZip, void **pBuf, size_t *pSize) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_writer_finalize_heap_archive out of Core 8 scope.");
#else
mz_bool mz_zip_writer_finalize_heap_archive(mz_zip_archive *pZip, void **pBuf, size_t *pSize);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_writer_init(mz_zip_archive *pZip, mz_uint64 existing_size) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_writer_init out of Core 8 scope.");
#else
mz_bool mz_zip_writer_init(mz_zip_archive *pZip, mz_uint64 existing_size);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_writer_init_file(mz_zip_archive *pZip, const char *pFilename, mz_uint64 size_to_reserve_at_beginning) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_writer_init_file out of Core 8 scope.");
#else
mz_bool mz_zip_writer_init_file(mz_zip_archive *pZip, const char *pFilename, mz_uint64 size_to_reserve_at_beginning);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_writer_init_from_reader(mz_zip_archive *pZip, const char *pFilename) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_writer_init_from_reader out of Core 8 scope.");
#else
mz_bool mz_zip_writer_init_from_reader(mz_zip_archive *pZip, const char *pFilename);
#endif

#if defined(__WINK_SIM__)
mz_bool mz_zip_writer_init_heap(mz_zip_archive *pZip, size_t size_to_reserve_at_beginning, size_t initial_allocation_size) WINK_SLA_ERROR("Wink SLA Violation: mz_zip_writer_init_heap out of Core 8 scope.");
#else
mz_bool mz_zip_writer_init_heap(mz_zip_archive *pZip, size_t size_to_reserve_at_beginning, size_t initial_allocation_size);
#endif

#if defined(__WINK_SIM__)
typedef size_t(*mz_file_read_func)(void *pOpaque, mz_uint64 file_ofs, void *pBuf, size_t n) WINK_SLA_ERROR("Wink SLA Violation: size_t out of Core 8 scope.");
#else
typedef size_t(*mz_file_read_func)(void *pOpaque, mz_uint64 file_ofs, void *pBuf, size_t n);
#endif

#if defined(__WINK_SIM__)
tdefl_status tdefl_compress(tdefl_compressor *d, const void *pIn_buf, size_t *pIn_buf_size, void *pOut_buf, size_t *pOut_buf_size, tdefl_flush flush) WINK_SLA_ERROR("Wink SLA Violation: tdefl_compress out of Core 8 scope.");
#else
tdefl_status tdefl_compress(tdefl_compressor *d, const void *pIn_buf, size_t *pIn_buf_size, void *pOut_buf, size_t *pOut_buf_size, tdefl_flush flush);
#endif

#if defined(__WINK_SIM__)
tdefl_status tdefl_compress_buffer(tdefl_compressor *d, const void *pIn_buf, size_t in_buf_size, tdefl_flush flush) WINK_SLA_ERROR("Wink SLA Violation: tdefl_compress_buffer out of Core 8 scope.");
#else
tdefl_status tdefl_compress_buffer(tdefl_compressor *d, const void *pIn_buf, size_t in_buf_size, tdefl_flush flush);
#endif

#if defined(__WINK_SIM__)
void * tdefl_compress_mem_to_heap(const void *pSrc_buf, size_t src_buf_len, size_t *pOut_len, int flags) WINK_SLA_ERROR("Wink SLA Violation: tdefl_compress_mem_to_heap out of Core 8 scope.");
#else
void * tdefl_compress_mem_to_heap(const void *pSrc_buf, size_t src_buf_len, size_t *pOut_len, int flags);
#endif

#if defined(__WINK_SIM__)
size_t tdefl_compress_mem_to_mem(void *pOut_buf, size_t out_buf_len, const void *pSrc_buf, size_t src_buf_len, int flags) WINK_SLA_ERROR("Wink SLA Violation: tdefl_compress_mem_to_mem out of Core 8 scope.");
#else
size_t tdefl_compress_mem_to_mem(void *pOut_buf, size_t out_buf_len, const void *pSrc_buf, size_t src_buf_len, int flags);
#endif

#if defined(__WINK_SIM__)
mz_bool tdefl_compress_mem_to_output(const void *pBuf, size_t buf_len, tdefl_put_buf_func_ptr pPut_buf_func, void *pPut_buf_user, int flags) WINK_SLA_ERROR("Wink SLA Violation: tdefl_compress_mem_to_output out of Core 8 scope.");
#else
mz_bool tdefl_compress_mem_to_output(const void *pBuf, size_t buf_len, tdefl_put_buf_func_ptr pPut_buf_func, void *pPut_buf_user, int flags);
#endif

#if defined(__WINK_SIM__)
mz_uint tdefl_create_comp_flags_from_zip_params(int level, int window_bits, int strategy) WINK_SLA_ERROR("Wink SLA Violation: tdefl_create_comp_flags_from_zip_params out of Core 8 scope.");
#else
mz_uint tdefl_create_comp_flags_from_zip_params(int level, int window_bits, int strategy);
#endif

#if defined(__WINK_SIM__)
mz_uint32 tdefl_get_adler32(tdefl_compressor *d) WINK_SLA_ERROR("Wink SLA Violation: tdefl_get_adler32 out of Core 8 scope.");
#else
mz_uint32 tdefl_get_adler32(tdefl_compressor *d);
#endif

#if defined(__WINK_SIM__)
tdefl_status tdefl_get_prev_return_status(tdefl_compressor *d) WINK_SLA_ERROR("Wink SLA Violation: tdefl_get_prev_return_status out of Core 8 scope.");
#else
tdefl_status tdefl_get_prev_return_status(tdefl_compressor *d);
#endif

#if defined(__WINK_SIM__)
tdefl_status tdefl_init(tdefl_compressor *d, tdefl_put_buf_func_ptr pPut_buf_func, void *pPut_buf_user, int flags) WINK_SLA_ERROR("Wink SLA Violation: tdefl_init out of Core 8 scope.");
#else
tdefl_status tdefl_init(tdefl_compressor *d, tdefl_put_buf_func_ptr pPut_buf_func, void *pPut_buf_user, int flags);
#endif

#if defined(__WINK_SIM__)
void * tdefl_write_image_to_png_file_in_memory(const void *pImage, int w, int h, int num_chans, size_t *pLen_out) WINK_SLA_ERROR("Wink SLA Violation: tdefl_write_image_to_png_file_in_memory out of Core 8 scope.");
#else
void * tdefl_write_image_to_png_file_in_memory(const void *pImage, int w, int h, int num_chans, size_t *pLen_out);
#endif

#if defined(__WINK_SIM__)
void * tdefl_write_image_to_png_file_in_memory_ex(const void *pImage, int w, int h, int num_chans, size_t *pLen_out, mz_uint level, mz_bool flip) WINK_SLA_ERROR("Wink SLA Violation: tdefl_write_image_to_png_file_in_memory_ex out of Core 8 scope.");
#else
void * tdefl_write_image_to_png_file_in_memory_ex(const void *pImage, int w, int h, int num_chans, size_t *pLen_out, mz_uint level, mz_bool flip);
#endif

#if defined(__WINK_SIM__)
tinfl_status tinfl_decompress(tinfl_decompressor *r, const mz_uint8 *pIn_buf_next, size_t *pIn_buf_size, mz_uint8 *pOut_buf_start, mz_uint8 *pOut_buf_next, size_t *pOut_buf_size, const mz_uint32 decomp_flags) WINK_SLA_ERROR("Wink SLA Violation: tinfl_decompress out of Core 8 scope.");
#else
tinfl_status tinfl_decompress(tinfl_decompressor *r, const mz_uint8 *pIn_buf_next, size_t *pIn_buf_size, mz_uint8 *pOut_buf_start, mz_uint8 *pOut_buf_next, size_t *pOut_buf_size, const mz_uint32 decomp_flags);
#endif

#if defined(__WINK_SIM__)
int tinfl_decompress_mem_to_callback(const void *pIn_buf, size_t *pIn_buf_size, tinfl_put_buf_func_ptr pPut_buf_func, void *pPut_buf_user, int flags) WINK_SLA_ERROR("Wink SLA Violation: tinfl_decompress_mem_to_callback out of Core 8 scope.");
#else
int tinfl_decompress_mem_to_callback(const void *pIn_buf, size_t *pIn_buf_size, tinfl_put_buf_func_ptr pPut_buf_func, void *pPut_buf_user, int flags);
#endif

#if defined(__WINK_SIM__)
void * tinfl_decompress_mem_to_heap(const void *pSrc_buf, size_t src_buf_len, size_t *pOut_len, int flags) WINK_SLA_ERROR("Wink SLA Violation: tinfl_decompress_mem_to_heap out of Core 8 scope.");
#else
void * tinfl_decompress_mem_to_heap(const void *pSrc_buf, size_t src_buf_len, size_t *pOut_len, int flags);
#endif

#if defined(__WINK_SIM__)
size_t tinfl_decompress_mem_to_mem(void *pOut_buf, size_t out_buf_len, const void *pSrc_buf, size_t src_buf_len, int flags) WINK_SLA_ERROR("Wink SLA Violation: tinfl_decompress_mem_to_mem out of Core 8 scope.");
#else
size_t tinfl_decompress_mem_to_mem(void *pOut_buf, size_t out_buf_len, const void *pSrc_buf, size_t src_buf_len, int flags);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_MINIZ_H__ */
#endif /* MINIZ_HEADER_INCLUDED */
