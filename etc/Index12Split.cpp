#include <stdio.h>
#include <stdint.h>

int index(int x, int y, int z)
{
    return z*4*4 + y*4 + x;
}

int main()
{
    Brick b;
    uint32_t dw[24];
    for (int z = 0; z < 4; ++z)
    {
        for (int y = 0; y < 4; ++y)
        {
            for (int x = 0; x < 4; ++x)
            {
                int i = b.index(x, y, z);
                int t = i / 8;
                int o = i % 8;
                int w = t * 3 + (y & 1);
                int u = w + 1 + !(y & 1);
                int q = x;
                int l = q*8;
                int h = q*4;
                uint32_t m = 0xffu << l;
                uint32_t n = 0xfu << h;
                //SET
                uint32_t v = i;
                uint32_t v1 = v & 0xff;
                uint32_t v2 = (v & 0xf00) >> 8;
                dw[w] = (dw[w] & ~(m)) | (v1 << l);
                dw[u] = (dw[u] & ~(n)) | (v2 << h);
                // GET
                int g = ((dw[w] & m) >> l) | (((dw[u] & n) >> h) << 8);

                printf("x=%d,y=%d,z=%d, i=%d,", x, y, z, i);
                printf("t=%d,o=%d,", t, o);
                printf("w=%d,q=%d,u=%d,", w, q, u);
                printf("l=%d,h=%d,m=%x,n=%x,", l, h, m, n);
                printf("dw[w]=%x,dw[u]=%x", dw[w], dw[u]);
                printf("v=%d,v1=%x,v2=%x,g=%d,", v, v1, v2, g);

                putchar('\n');
            }
        }
    }

    for (int i = 0; i < 64; ++i)
        printf("dw[%d]=%x\n", i, dw[i]);

    return 0;
}
