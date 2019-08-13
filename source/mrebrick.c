/*
 *   Module to implement Comb method for fast
 *   computation of x*G mod n, for fixed G and n, using precomputation. 
 *
 *   Elliptic curve version of mrbrick.c
 *
 *   This idea can be used to substantially speed up certain phases 
 *   of the Digital Signature Standard (ECS) for example.
 *
 *   See "Handbook of Applied Cryptography"
 *
 *   Copyright (c) 1988-2006 Shamus Software Ltd.
 */

#include <stdlib.h> 
#include "miracl.h"
#ifdef MR_STATIC
#include <string.h>
#endif

#ifndef MR_STATIC

BOOL ebrick_init(_MIPD_ ebrick *B,big x,big y,big a,big b,big n,int window,int nb)
{ /* Uses Montgomery arithmetic internally              *
   * (x,y) is the fixed base                            *
   * a,b and n are parameters and modulus of the curve  *
   * window is the window size in bits and              *
   * nb is the maximum number of bits in the multiplier */
    int i,j,k,t,bp,len,bptr;
    epoint **table;
    epoint *w;

#ifdef MR_OS_THREADS
    miracl *mr_mip=get_mip();
#endif
    if (nb<2 || window<1 || window>nb || mr_mip->ERNUM) return FALSE;

    t=MR_ROUNDUP(nb,window);
    if (t<2) return FALSE;

    MR_IN(115)

#ifndef MR_ALWAYS_BINARY
    if (mr_mip->base != mr_mip->base2)
    {
        mr_berror(_MIPP_ MR_ERR_NOT_SUPPORTED);
        MR_OUT
        return FALSE;
    }
#endif

    B->window=window;
    B->max=nb;
    table=mr_alloc(_MIPP_ (1<<window),sizeof(epoint *));
    if (table==NULL)
    {
        mr_berror(_MIPP_ MR_ERR_OUT_OF_MEMORY);   
        MR_OUT
        return FALSE;
    }
    B->a=mirvar(_MIPP_ 0);
    B->b=mirvar(_MIPP_ 0);
    B->n=mirvar(_MIPP_ 0);
    copy(a,B->a);
    copy(b,B->b);
    copy(n,B->n);

    ecurve_init(_MIPP_ a,b,n,MR_AFFINE);
    w=epoint_init(_MIPPO_ );
    epoint_set(_MIPP_ x,y,0,w);
    table[0]=epoint_init(_MIPPO_ );
    table[1]=epoint_init(_MIPPO_ );
    epoint_copy(w,table[1]);
    for (j=0;j<t;j++)
        ecurve_double(_MIPP_ w);

    k=1;
    for (i=2;i<(1<<window);i++)
    {
        table[i]=epoint_init(_MIPPO_ );
        if (i==(1<<k))
        {
            k++;
            epoint_copy(w,table[i]);
            
            for (j=0;j<t;j++)
                ecurve_double(_MIPP_ w);
            continue;
        }
        bp=1;
        for (j=0;j<k;j++)
        {
            if (i&bp)
                ecurve_add(_MIPP_ table[1<<j],table[i]);
            bp<<=1;
        }
    }
    epoint_free(w);

/* create the table */

    len=n->len;
    bptr=0;
    B->table=mr_alloc(_MIPP_ 2*len*(1<<window),sizeof(mr_small));

    for (i=0;i<(1<<window);i++)
    {
        for (j=0;j<len;j++)
        {
            B->table[bptr++]=table[i]->X->w[j];
        }
        for (j=0;j<len;j++)
        {
            B->table[bptr++]=table[i]->Y->w[j];
        }

        epoint_free(table[i]);
    }
        
    mr_free(table);  

    MR_OUT
    return TRUE;
}

void ebrick_end(ebrick *B)
{
    mirkill(B->n);
    mirkill(B->b);
    mirkill(B->a);
    mr_free(B->table);  
}

#else

/* use precomputated table in ROM - see ebrick2.c for example of how to create the table, and ecdh.c 
   for an example of use */

void ebrick_init(ebrick *B,const mr_small* rom,big a,big b,big n,int window,int nb)
{
    B->table=rom;
    B->a=a; /* just pass a pointer */
    B->b=b;
    B->n=n;
    B->window=window;  /* 2^4=16  stored values */
    B->max=nb;
}

#endif

int mul_brick(_MIPD_ ebrick *B,big e,big x,big y)
{
    int i,j,t,d,len,maxsize,promptr;
    epoint *w,*z;
 
#ifdef MR_STATIC
    char mem[MR_ECP_RESERVE(2)];
#else
    char *mem;
#endif
#ifdef MR_OS_THREADS
    miracl *mr_mip=get_mip();
#endif

    if (size(e)<0) mr_berror(_MIPP_ MR_ERR_NEG_POWER);
    t=MR_ROUNDUP(B->max,B->window);
    
    MR_IN(116)

#ifndef MR_ALWAYS_BINARY
    if (mr_mip->base != mr_mip->base2)
    {
        mr_berror(_MIPP_ MR_ERR_NOT_SUPPORTED);
        MR_OUT
        return 0;
    }
#endif

    if (logb2(_MIPP_ e) > B->max)
    {
        mr_berror(_MIPP_ MR_ERR_EXP_TOO_BIG);
        MR_OUT
        return 0;
    }

    ecurve_init(_MIPP_ B->a,B->b,B->n,MR_BEST);
#ifdef MR_STATIC
    memset(mem,0,MR_ECP_RESERVE(2));
#else
    mem=ecp_memalloc(_MIPP_ 2);
#endif
    w=epoint_init_mem(_MIPP_ mem,0);
    z=epoint_init_mem(_MIPP_ mem,1);

    len=B->n->len;
    maxsize=2*(1<<B->window)*len;

    j=recode(_MIPP_ e,t,B->window,t-1);
    if (j>0)
    {
        promptr=2*j*len;
        init_point_from_rom(w,len,B->table,maxsize,&promptr);
    }

    for (i=t-2;i>=0;i--)
    {
        j=recode(_MIPP_ e,t,B->window,i);
        ecurve_double(_MIPP_ w);

        if (j>0)
        {
            promptr=2*j*len;
            init_point_from_rom(z,len,B->table,maxsize,&promptr);
            ecurve_add(_MIPP_ z,w);
        }
    }

    d=epoint_get(_MIPP_ w,x,y);
#ifndef MR_STATIC
    ecp_memkill(_MIPP_ mem,2);
#else
    memset(mem,0,MR_ECP_RESERVE(2));
#endif
    MR_OUT
    return d;
}
