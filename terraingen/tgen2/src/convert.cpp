#include "pe-parse/parser-library/parse.h"
#include "undname/undname.h"
#include <stdio.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <assert.h>
#include "mem.h"
#include "config.h"
#include <stdio.h>
#include "call.h"
#include <vector>
#include <string.h>
#include "tgen.h"

using namespace peparse;

struct Section
{
    std::string name;
    VA start;
    VA end;
    char * data;
};

struct Reloc
{
    VA addr;
    uint32_t value;
};

static VA imagebase;
static VA entrypoint;
static std::vector<Section> sections;

static Section * get_section(VA addr);

static void * load_x86(const void * p, unsigned int size)
{
    void * f = alloc_exec(size);
    memcpy(f, p, size);
    return f;
}

static uint32_t get_dword_va(VA addr)
{
    void * p = get_mem_va(addr);
    uint32_t v;
    memcpy(&v, p, sizeof(uint32_t));
    return v;
}

static void * get_mem(uint32_t v)
{
    return (void*)v;
}

#ifdef _WIN32
#ifdef IS_X64
#include "../tools/wrapper_msvc_x64.cpp"
#else
#include "../tools/wrapper_msvc.cpp"
#endif
#else
#ifdef IS_X64
#include "../tools/wrapper_x64.cpp"
#else
#include "../tools/wrapper.cpp"
#endif
#endif

template <class T>
char * resolve_import(T & imp)
{
#ifdef IS_X64
    char * x86 = (char*)load_x86(imp.asm_data, imp.asm_size);
    for (unsigned int i = 0; i < imp.asm_size - 8; ++i) {
        uint64_t v = *((uint64_t*)&x86[i]);
        if (v != 0x1122334455667788)
            continue;
        *((uint64_t*)&x86[i]) = (uint64_t)imp.c_func;
        break;
    }
#else
    char * x86 = (char*)imp.c_func;
#endif
    return x86;
}

int do_imports(void *N, VA impAddr, std::string &modName, std::string &symName)
{
    static unsigned int import_num = 0;
    char demangled[1024];
    char * res = __unDName(demangled, symName.c_str(), sizeof(demangled),
                                                 UNDNAME_COMPLETE);
#ifndef NDEBUG
    // std::cout << "0x" << to_string<VA>(impAddr, std::hex);
    std::cout << import_num << ": " << modName << "|" <<
        symName << "!" << demangled;
    std::cout << std::endl;
#endif

    auto it = imports.find(demangled);
    if (it != imports.end()) {
        Import & imp = it->second;
        char * x86 = resolve_import(imp);
        *(uint32_t*)get_mem_va(impAddr) = (uint32_t)x86;
    } else {
        *(uint32_t*)get_mem_va(impAddr) = import_num | 0x12340000;
    }
    import_num++;
    return 0;
}

void * get_mem_va(uint32_t addr)
{
    Section * s = get_section(addr);
    unsigned int offset = (unsigned int)(addr - s->start);
    return s->data + offset;
}

int do_relocs(void *N, VA relocAddr, reloc_type type)
{
    if (type == ABSOLUTE)
        return 0;
    if (type != HIGHLOW) {
        std::cout << "Unsupported loctype: " << type << '\n';
        return 0;
    }

    uint32_t dw = get_dword_va(relocAddr);
    if (dw == imagebase ||
        dw == imagebase+0x3c ||
        dw == imagebase+0x18 ||
        dw == imagebase+0x74 ||
        dw == imagebase+0xe8)
    {
        // dos headers
        return 0;
    }
    Section * s = get_section(dw);
    bool is_text = false;
    if (s->name == ".text")
        is_text = true;

    Section * s2 = get_section(dw);
    *(uint32_t*)get_mem_va(relocAddr) = (uint32_t)get_mem_va(dw);
    return 0;
}

static Section * get_section(VA addr)
{
    std::vector<Section>::iterator it;
    for (it = sections.begin(); it != sections.end(); ++it) {
        Section & s = *it;
        if (addr >= s.start && addr < s.end)
            return &s;
    }
    assert(false);
    return NULL;
}

static Section * get_section(const char * name)
{
    std::vector<Section>::iterator it;
    for (it = sections.begin(); it != sections.end(); ++it) {
        Section & s = *it;
        if (s.name == name)
            return &s;
    }
    assert(false);
    return NULL;
}

static void patch_text(char * in_data, unsigned int size)
{
    // 64 ff 35 00 00 00 00 -> push fs:00
    // to
    // FF 35 78 56 34 12 90 -> push [0x12345678]

    // 64 89 0D 00 00 00 00 -> mov fs:0, ecx
    // to
    // 89 0D 78 56 34 12 90 -> mov [0x12345678], ecx

    // 64 A3 00 00 00 00 -> mov fs:0x0, eax
    // to
    // A3 78 56 34 12 90 -> mov [somevar], eax

    // 64 A1 00 00 00 00 -> mov eax, fs:0
    // to
    // A1 78 56 34 12 90 -> mov eax, [0x12345678]

    void * fs = alloc_mem(4);
    unsigned int fsdw = (unsigned int)fs;

    unsigned char * data = (unsigned char*)in_data;
    for (unsigned int i = 0; i < size - 7; ++i) {
        if (data[i] == 0x64 && data[i+1] == 0xff && data[i+2] == 0x35 &&
            data[i+3] == 0x00 && data[i+4] == 0x00 &&
            data[i+5] == 0x00 && data[i+6] == 0x00)
        {
            data[i] = 0xFF;
            data[i+1] = 0x35;
            *(unsigned int*)&data[i+2] = fsdw;
            data[i+6] = 0x90;
            continue;
        }
        if (data[i] == 0x64 && data[i+1] == 0x89 && data[i+2] == 0x0D &&
            data[i+3] == 0x00 && data[i+4] == 0x00 &&
            data[i+5] == 0x00 && data[i+6] == 0x00)
        {
            data[i] = 0x89;
            data[i+1] = 0x0D;
            *(unsigned int*)&data[i+2] = fsdw;
            data[i+6] = 0x90;
            continue;
        }
        if (data[i] == 0x64 && data[i+1] == 0xA3 &&
            data[i+2] == 0x00 && data[i+3] == 0x00 &&
            data[i+4] == 0x00 && data[i+5] == 0x00)
        {
            data[i] = 0xA3;
            *(unsigned int*)&data[i+1] = fsdw;
            data[i+5] = 0x90;
            continue;
        }
        if (data[i] == 0x64 && data[i+1] == 0xA1 &&
            data[i+2] == 0x00 && data[i+3] == 0x00 &&
            data[i+4] == 0x00 && data[i+5] == 0x00)
        {
            data[i] = 0xA1;
            *(unsigned int*)&data[i+1] = fsdw;
            data[i+5] = 0x90;
            continue;
        }
    }
}

int do_secs(void *N, VA secBase,
            std::string &secName, image_section_header s,
            bounded_buffer *data)
{
    bool exec = false;
    bool readonly = false;
    if (secName == ".text") {
        exec = true;
        readonly = true;
    } else if (secName == ".rdata") {
        readonly = true;
    }

    char * sec_data;
    if (exec)
        sec_data = (char*)alloc_exec(s.Misc.VirtualSize);
    else
        sec_data = (char*)alloc_mem(s.Misc.VirtualSize); 
    memset(sec_data, 0, s.Misc.VirtualSize);
    memcpy(sec_data, data->buf,
           data->bufLen > s.Misc.VirtualSize ? s.Misc.VirtualSize :
                                               data->bufLen);

    if (exec)
        patch_text(sec_data, s.Misc.VirtualSize);

    sections.emplace_back(
        Section{secName, secBase, secBase + s.Misc.VirtualSize, sec_data});
    int64_t offset = (uint64_t)sec_data - (uint64_t)secBase;
#ifndef NDEBUG
    std::cout << "Sec Name: " << secName << std::endl;
    std::cout << "New offset: " << (uint64_t)sec_data << '\n';
    std::cout << "Offset: " << offset << '\n';
#endif

    return 0;
}

static void * get_entry_point()
{
    // 0:  53                      push   ebx
    // 1:  57                      push   edi
    // 2:  56                      push   esi
    // 3:  b8 44 33 22 11          mov    eax,0x11223344
    // 8:  6a 00                   push   0x0
    // a:  6a 00                   push   0x0
    // c:  6a 00                   push   0x0
    // e:  ff d0                   call   eax
    // 10: 83 c4 0c                add    esp,0xc
    // 13: 5e                      pop    esi
    // 14: 5f                      pop    edi
    // 15: 5b                      pop    ebx
    // 16: c3                      ret
    void * p = get_mem_va(SERVER_ENTRYPOINT);
    uint8_t * x86_f = (uint8_t*)alloc_exec(23);
    uint8_t * x86 = x86_f;
    *x86++ = 0x53;
    *x86++ = 0x57;
    *x86++ = 0x56;

    *x86++ = 0xB8;
    *(uint32_t*)x86 = (uint32_t)p;
    x86 += 4;

    *x86++ = 0x6A; *x86++ = 00;
    *x86++ = 0x6A; *x86++ = 00;
    *x86++ = 0x6A; *x86++ = 00;

    *x86++ = 0xFF; *x86++ = 0xD0;
    *x86++ = 0x83; *x86++ = 0xC4; *x86++ = 0x0C;
    *x86++ = 0x5E;
    *x86++ = 0x5F;
    *x86++ = 0x5B;
    *x86++ = 0xC3;
    return x86_f;
}

static void do_patches()
{

    for (Patch & patch : patches) {
        // push   0x11223344, ret -> 68 44 33 22 11 C3
        char * x86 = resolve_import(patch);
        uint8_t * p = NULL;
        if (patch.patch_table != 0) {
            uint32_t addr = *(uint32_t*)get_mem_va(patch.patch_table);
            p = (uint8_t*)addr;
        } else if (patch.patch_addr != 0) {
            p = (uint8_t*)get_mem_va(patch.patch_addr);
        }
        assert(p != NULL);
        *p = 0x68;
        p++;
        *(uint32_t*)p = (uint32_t)x86;
        p += 4;
        *p = 0xC3;
    }

    for (DirectPatch & patch : direct_patches) {
        uint8_t * p = (uint8_t*)get_mem_va(patch.patch_addr);
        assert(p != NULL);
        memcpy(p, patch.asm_data, patch.asm_size);
    }

    // mov esp, ebp -> 89 ec
    // pop ebp -> 5d
    // ret -> c3
    uint8_t* p = (uint8_t*)get_mem_va(SERVER_ENTRYPOINT_END);
    p[0] = 0x89;
    p[1] = 0xEC;
    p[2] = 0x5D;
    p[3] = 0xC3;
}

static void init_static()
{
#ifndef NDEBUG
    std::cout << "init static\n";
#endif

    unsigned int i = 0;
    for (VA p = STATIC_START; p < STATIC_END; p += 4, ++i) {
#ifndef NDEBUG
        std::cout << "static: " << p << '\n';
#endif
        uint32_t dw = get_dword_va(p);
#ifndef NDEBUG
        std::cout << "static dw: " << dw << '\n';
#endif
        if (has_ignore_static(i)) {
            continue;
        }
        void * fp = (void*)get_mem(dw);
#ifndef NDEBUG
        std::cout << "fp: " << (uintptr_t)fp << '\n';
#endif
        call_x86_cdecl_0(fp);
#ifndef NDEBUG
        std::cout << "called\n";
#endif
    }

#ifndef NDEBUG
    std::cout << "init static done\n";
#endif
}

extern const char * translate_path(char * v);

extern Heap main_heap;

static void do_heap_init(Heap * heap)
{
#ifndef NDEBUG
    std::cout << "do entry point\n";
#endif

    call_x86_cdecl_0(get_entry_point());

#ifndef NDEBUG
    std::cout << "done\n";
#endif
}

void real_main()
{
#ifndef NDEBUG
    std::cout << "real_main()\n";
#endif
    const char * server_path = translate_path("Server.exe");
    parsed_pe * p = ParsePEFromFile(server_path);
    if (p == NULL) {
        std::cout << "Could not find Server.exe at " << server_path << '\n';
    }
    imagebase = p->peHeader.nt.OptionalHeader.ImageBase;
    GetEntryPoint(p, entrypoint);

    IterSec(p, do_secs, NULL);
    IterImpVAString(p, do_imports, NULL);
    IterRelocs(p, do_relocs, NULL);

    do_patches();

    create_heap(&main_heap, MAIN_HEAP_SIZE);
    init_static();
    do_heap_init(&main_heap);
}

void tgen_init()
{
    static bool initialized = false;
    if (initialized)
        return;
#ifdef IS_X64
    SETUP_CALLERS();
#endif
    run_with_stack(real_main);
    initialized = true;
}

static void dummy(Creature * addr)
{
    std::cout << "creature: " << (uint32_t)addr << '\n';
}

int main(int argc, char * argv[])
{
    uint32_t x = 32802;
    uint32_t y = 32803;
    std::cout << "set seed" << '\n';
    void tgen_set_seed(uint32_t seed);
    tgen_set_seed(26879);
    std::cout << "init" << '\n';
    tgen_init();
    std::cout << "generate" << '\n';
    tgen_generate_chunk(x, y);
    tgen_destroy_chunk(x, y);
    uint32_t rx = x / 64;
    uint32_t ry = y / 64;
    Region * r = tgen_get_region(tgen_get_manager(), rx, ry);

    unsigned int regseed_count = 0;
    for (int x = -20; x < 20; ++x)
    for (int y = -20; y < 20; ++y) {
        if (*tgen_get_region_seed_ptr(tgen_get_manager(), rx+x, ry+y))
            regseed_count++;
    }
    std::cout << "regseed_count: " << regseed_count << '\n';

    Zone * z = tgen_get_zone(r, x, y);
    std::cout << "done" << '\n';

    Creature * c = sim_add_creature(0);
    EntityData * e = &c->entity_data;
    e->hostile_type = 0;
    e->pos[0] = 32802ULL * 0x1000000ULL;
    e->pos[1] = 32803ULL * 0x1000000ULL;
    e->pos[2] = 0;

    while (1) {
        sim_step(20);
        sim_get_creatures(dummy);
    }
}
