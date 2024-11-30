#include <stdio.h>
#include <stdint.h>

#if defined(_MSC_VER)
#include <intrin.h> // For __cpuid on MSVC
#else
#include <cpuid.h>  // For __get_cpuid on GCC/Clang
#endif

void get_cpuid(int info[4], int function_id) {
#if defined(_MSC_VER)
    __cpuid(info, function_id);
#else
    __get_cpuid(function_id, (unsigned int *)&info[0], (unsigned int *)&info[1], (unsigned int *)&info[2], (unsigned int *)&info[3]);
#endif
}

int main() {
    int cpu_info[4] = {0};
    get_cpuid(cpu_info, 0); // Query the highest supported function ID
    int max_function_id = cpu_info[0];

    // Get vendor string
    char vendor[13] = {0};
    ((int *)vendor)[0] = cpu_info[1]; // EBX
    ((int *)vendor)[1] = cpu_info[3]; // EDX
    ((int *)vendor)[2] = cpu_info[2]; // ECX
    printf("CPU Vendor: %s\n", vendor);

    if (max_function_id >= 1) {
        get_cpuid(cpu_info, 1); // Query feature flags

        int sse2_supported = cpu_info[3] & (1 << 26); // Check bit 26 in EDX
        int avx_supported = cpu_info[2] & (1 << 28);  // Check bit 28 in ECX
        int osxsave_supported = cpu_info[2] & (1 << 27); // Check bit 27 in ECX

        printf("SSE2 Support: %s\n", sse2_supported ? "Yes" : "No");
        printf("AVX Support: %s\n", (avx_supported && osxsave_supported) ? "Yes" : "No");

        // Check AVX OS support
        if (avx_supported && osxsave_supported) {
            uint64_t xcr0;
#if defined(_MSC_VER)
            xcr0 = _xgetbv(0); // Get XCR0 on MSVC
#else
            __asm__ __volatile__("xgetbv" : "=a"(xcr0) : "c"(0) : "%edx");
#endif
            if ((xcr0 & 0x6) == 0x6) {
                printf("AVX is usable by the OS.\n");
            } else {
                printf("AVX is not enabled by the OS.\n");
            }
        }
    } else {
        printf("CPUID function 1 is not supported.\n");
    }

    // Additional checks could include CPU model/family
    return 0;
}

