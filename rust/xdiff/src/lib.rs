pub mod xutils;

pub const XDF_IGNORE_WHITESPACE: u64 = 1 << 1;
pub const XDF_IGNORE_WHITESPACE_CHANGE: u64 = 1 << 2;
pub const XDF_IGNORE_WHITESPACE_AT_EOL: u64 = 1 << 3;
pub const XDF_IGNORE_CR_AT_EOL: u64 = 1 << 4;
pub const XDF_WHITESPACE_FLAGS: u64 = XDF_IGNORE_WHITESPACE |
    XDF_IGNORE_WHITESPACE_CHANGE |
    XDF_IGNORE_WHITESPACE_AT_EOL |
    XDF_IGNORE_CR_AT_EOL;


#[no_mangle]
unsafe extern "C" fn xxh3_64(ptr: *const u8, size: usize) -> u64 {
    let slice = std::slice::from_raw_parts(ptr, size);
    xxhash_rust::xxh3::xxh3_64(slice)
}
