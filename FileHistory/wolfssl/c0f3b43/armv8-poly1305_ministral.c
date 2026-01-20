Do NOT include any explanation or comments.

```c
static WC_INLINE void poly1305_blocks_aarch64_16(Poly1305* ctx,
    const unsigned char *m, size_t bytes)
{
    __asm__ __volatile__ (
        /* Check for zero bytes to do. */
        "CMP        %[bytes], %[POLY1305_BLOCK_SIZE] \n\t"
        "BLO        L_poly1305_aarch64_16_done_%= \n\t"

        "MOV        x12, #1               \n\t"
        /* Load h */
        "LDP        w4, w5, [%[ctx_h], #0]   \n\t"
        "LDP        w6, w7, [%[ctx_h], #8]   \n\t"
        "LDR        w8, [%[ctx_h], #16]   \n\t"
        /* Base 26 -> Base 64 */
        "ORR        x4, x4, x5, LSL #26\n\t"
        "ORR        x4, x4, x6, LSL #52\n\t"
        "LSR        x5, x6, #12\n\t"
        "ORR        x5, x5, x7, LSL #14\n\t"
        "ORR        x5, x5, x8, LSL #40\n\t"
        "LSR        x6, x8, #24\n\t"
        /* Load r */
        "LDP        x8, x9, %[ctx_r64]   \n\t"
        "SUB        %[finished], x12, %[finished]\n\t"
        "\n"
        ".align 2 \n\t"
    "L_poly1305_aarch64_16_loop_%=: \n\t"
        /* Load m */
        "LDR        x10, [%[m]]          \n\t"
        "LDR        x11, [%[m], 8]       \n\t"
        /* Add m and !finished at bit 128. */
        "ADDS       x4, x4, x10          \n\t"
        "ADCS       x5, x5, x11          \n\t"
        "ADC        x6, x6, %[finished]  \n\t"

        /* r * h */
        /* r0 * h0 */
        "MUL        x12, x8, x4\n\t"
        "UMULH      x13, x8, x4\n\t"
        /* r0 * h1 */
        "MUL        x16, x8, x5\n\t"
        "UMULH      x14, x8, x5\n\t"
        /* r1 * h0 */
        "MUL        x15, x9, x4\n\t"
        "ADDS       x13, x13, x16\n\t"
        "UMULH      x17, x9, x4\n\t"
        "ADC        x14, x14, xzr\n\t"
        "ADDS       x13, x13, x15\n\t"
        /* r0 * h2 */
        "MUL        x16, x8, x6\n\t"
        "ADCS       x14, x14, x17\n\t"
        "UMULH      x17, x8, x6\n\t"
        "ADC        x15, xzr, xzr\n\t"
        "ADDS       x14, x14, x16\n\t"
        /* r1 * h1 */
        "MUL        x16, x9, x5\n\t"
        "ADC        x15, x15, x17\n\t"
        "UMULH      x19, x9, x5\n\t"
        "ADDS       x14, x14, x16\n\t"
        /* r1 * h2 */
        "MUL        x17, x9, x6\n\t"
        "ADCS       x15, x15, x19\n\t"
        "UMULH      x19, x9, x6\n\t"
        "ADC        x16, xzr, xzr\n\t"
        "ADDS       x15, x15, x17\n\t"
        "ADC        x16, x16, x19\n\t"
        /* h' = x12, x13, x14, x15, x16 */

        /* h' mod 2^130 - 5 */
        /* Get top two bits from h[2]. */
        "AND        x6, x14, 3\n\t"
        /* Get high bits from h[2]. */
        "AND        x14, x14, -4\n\t"
        /* Add top bits * 4. */
        "ADDS       x4, x12, x14\n\t"
        "ADCS       x5, x13, x15\n\t"
        "ADC        x6, x6, x16\n\t"
        /* Move down 2 bits. */
        "EXTR       x14, x15, x14, 2\n\t"
        "EXTR       x15, x16, x15, 2\n\t"
        /* Add top bits. */
        "ADDS       x4, x4, x14\n\t"
        "ADCS       x5, x5, x15\n\t"
        "ADC        x6, x6, xzr\n\t"

        "SUBS       %[bytes], %[bytes], %[POLY1305_BLOCK_SIZE]\n\t"
        "ADD        %[m], %[m], %[POLY1305_BLOCK_SIZE]\n\t"
        "BGT        L_poly1305_aarch64_16_loop_%=\n\t"

        /* Base 64 -> Base 26 */
        "MOV        x10, #0x3ffffff\n\t"
        "EXTR       x8, x6, x5, #40\n\t"
        "AND        x7, x10, x5, LSR #14\n\t"
        "EXTR       x6, x5, x4, #52\n\t"
        "AND        x5, x10, x4, LSR #26\n\t"
        "AND        x4, x4, x10\n\t"
        "AND        x6, x6, x10\n\t"
        "AND        x8, x8, x10\n\t"
        "STP        w4, w5, [%[ctx_h], #0]   \n\t"
        "STP        w6, w7, [%[ctx_h], #8]   \n\t"
        "STR        w8, [%[ctx_h], #16]   \n\t"
        "\n"
        ".align 2 \n\t"
    "L_poly1305_aarch64_16_done_%=: \n\t"
        : [bytes] "+r" (bytes), [m] "+r" (m)
        : [POLY1305_BLOCK_SIZE] "I" (POLY1305_BLOCK_SIZE),
          [ctx_r64] "m" (ctx->r64[0]), [ctx_h] "r" (ctx->h),
          [finished] "r" ((word64)ctx->finished)
        : "memory", "cc",
          "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14",
          "x15", "x16", "x17", "x19"
    );
}
```
