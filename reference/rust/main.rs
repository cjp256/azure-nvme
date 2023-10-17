extern crate libc;
extern crate nix;

use libc::{c_ulong, c_void, posix_memalign, sysconf, _SC_PAGESIZE};
use nix::fcntl::open;
use nix::fcntl::OFlag;
use nix::sys::stat::Mode;
use nix::unistd::close;
use std::env;
use std::path::Path;
use std::ptr::null_mut;

const NVME_IOCTL_ADMIN_CMD: c_ulong = 0xC0484E41;

#[repr(C)]
#[derive(Default)]
struct NvmeAdminCmd {
    opcode: u8,
    flags: u8,
    rsvd1: u16,
    nsid: u32,
    cdw2: u32,
    cdw3: u32,
    metadata: u64,
    addr: u64,
    metadata_len: u32,
    data_len: u32,
    cdw10: u32,
    cdw11: u32,
    cdw12: u32,
    cdw13: u32,
    cdw14: u32,
    cdw15: u32,
    timeout_ms: u32,
    result: u32,
}

#[repr(C)]
struct NvmeLbaf {
    ms: u16,
    ds: u8,
    rp: u8,
}

#[repr(C)]
struct NvmeIdNs {
    nsze: u64,
    ncap: u64,
    nuse: u64,
    nsfeat: u8,
    nlbaf: u8,
    flbas: u8,
    mc: u8,
    dpc: u8,
    dps: u8,
    nmic: u8,
    rescap: u8,
    fpi: u8,
    dlfeat: u8,
    nawun: u16,
    nawupf: u16,
    nacwu: u16,
    nabsn: u16,
    nabo: u16,
    nabspf: u16,
    noiob: u16,
    nvmcap: [u8; 16],
    npwg: u16,
    npwa: u16,
    npdg: u16,
    npda: u16,
    nows: u16,
    rsvd74: [u8; 18],
    anagrpid: u32,
    rsvd96: [u8; 3],
    nsattr: u8,
    nvmsetid: u16,
    endgid: u16,
    nguid: [u8; 16],
    eui64: [u8; 8],
    lbaf: [NvmeLbaf; 16],
    rsvd192: [u8; 192],
    vs: [u8; 3712],
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!("error: specify device");
        std::process::exit(1);
    }

    let device = Path::new(&args[1]);
    let fd = open(device, OFlag::O_RDONLY, Mode::empty())?;

    let mut ns = null_mut::<NvmeIdNs>();
    let align = unsafe { sysconf(_SC_PAGESIZE) } as usize;
    let ret = unsafe {
        posix_memalign(
            &mut ns as *mut _ as *mut *mut c_void,
            align,
            std::mem::size_of::<NvmeIdNs>(),
        )
    };

    if ret != 0 {
        Err("error: posix_memalign failed")?;
    }

    let cmd = NvmeAdminCmd {
        opcode: 0x06,
        nsid: 1,
        addr: ns as u64,
        data_len: std::mem::size_of::<NvmeIdNs>() as u32,
        ..Default::default()
    };

    let ret = unsafe { libc::ioctl(fd, NVME_IOCTL_ADMIN_CMD, &cmd) };
    if ret < 0 {
        Err("error: ioctl failed")?;
    }

    close(fd)?;

    let ns_ref = unsafe { &*ns };
    if ns_ref.vs[0] != 0 {
        let vs_str = std::str::from_utf8(&ns_ref.vs)?;
        println!("{}", vs_str);
    } else {
        eprintln!("error: no identifier found");
    }

    unsafe {
        libc::free(ns as *mut c_void);
    }

    Ok(())
}
