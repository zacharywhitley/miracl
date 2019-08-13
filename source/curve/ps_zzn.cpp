/*
 * C++ class to implement a power series type and to allow 
 * arithmetic on it
 *
 * WARNING: This class has been cobbled together for a specific use with
 * the MIRACL library It is not complete, and may not work in other 
 * applications
 *
 * See Knuth The Art of Computer Programming Vol.2, Chapter 4.7 
 */

#include "ps_zzn.h"

#define FFT

//
// all calulations are mod x^psN
// Power series is stored {as offset, pwr and a0+a1.x+a2.x^2+a3.x^3....}
// where the power of x is to be multiplied by pwr, and the whole power
// series is to be divided by x^offset
//

int psN;

//
// Copy Constructor
//

Ps_ZZn::Ps_ZZn(const Ps_ZZn& p)
{
    term_ps_zzn *ptr=p.start;
    term_ps_zzn *pos=NULL; 
    int pw;
    start=NULL;
    offset=p.offset;
    pwr=p.pwr;
    while (ptr!=NULL)
    {
        pw=ptr->n*p.pwr-p.offset;   // conversion needed
        if (pw>=psN) break;
        pos=addterm(ptr->an,pw,pos);
        ptr=ptr->next;
    }
}

//
// decompresses PS by reducing pwr
//

void Ps_ZZn::decompress(int m)
{ // m is divisor of current pwr
  // e.g pwr is 6 and PS = 1 + x + x^2
  // If m=2 then pwr becomes 3 and PS = 1 +x^2 +x^4.... 
    term_ps_zzn *ptr=start;
    if (start==NULL || m==1 || pwr==1) return; // it is fully decompressed
    while (ptr!=NULL)
    {
        ptr->n*=m;
        ptr=ptr->next;
    }
    pwr/=m;
}

//
//  Sets new pwr value. Parameter must exactly divide 
//  all powers in the series
//

void Ps_ZZn::compress(int p)
{
    term_ps_zzn *ptr=start;
    if (p==1) return;
    while (ptr!=NULL)
    {
        ptr->n/=p;
        ptr=ptr->next;  
    }
    pwr=p;
}

//
// insert missing terms with 0 coefficients
//

void Ps_ZZn::pad()
{ // insert any missing 0 coefficients 
    int i=0;
    term_ps_zzn *ptr=start;
    term_ps_zzn *pos=NULL;
    while (ptr!=NULL)
    {
        while (i<ptr->n)
        {
            pos=addterm((ZZn)0,i*pwr-offset,pos);
            i++;
        }
        i++;
        ptr=ptr->next;
    }
    while (i<psN/pwr) 
    {
        pos=addterm((ZZn)0,i*pwr-offset,pos);
        i++;
    }
}

//
// Find max coefficient in PS
//

int Ps_ZZn::max()
{
    int b,m=0;
    term_ps_zzn *ptr=start;
    while (ptr!=NULL)
    {
        b=bits(ptr->an);
        if (b>m) m=b;
        ptr=ptr->next;
    }
    return m;
}

//
// set new offset, so first power of x is 0
//

void Ps_ZZn::norm()
{
    int m;
    term_ps_zzn *ptr;
    if (start==NULL) return;
//
// remove any leading 0 terms
//
    while (start->an.iszero()) 
    {
        ptr=start->next;
        delete start;
        start=ptr;
        if (start==NULL) return;
    }
    ptr=start;
    m=start->n;
    if (m!=0)
    {
        offset-=m*pwr;
        while (ptr!=NULL)
        {
            ptr->n-=m;
            ptr=ptr->next;
        }
    }
}

//
// Dedekind Eta function (1-x)(1-x^2)(1-x^3)....
//

Ps_ZZn eta()
{ // simple repeating pattern
    BOOL even;
    int one,ce,co,c;
    term_ps_zzn *pos=NULL;
    Ps_ZZn n;
    n.addterm((ZZn)1,0);
    n.addterm((ZZn)-1,1);
    n.addterm((ZZn)-1,2); 
    ce=2;co=1;
    even=TRUE;
    c=2;
    one=1;
    while (c<psN)
    {
        if (even)
        {
            c+=(ce+1);
            ce+=2;
            pos=n.addterm((ZZn)one,c,pos);
            even=FALSE;
        }
        else
        {
            c+=(co+1);
            co+=1;
            pos=n.addterm((ZZn)one,c,pos);
            even=TRUE;
            one=(-one);
        }
    }
    return n;
}

//
// Checks if a power series is 0 or an integer
//

BOOL Ps_ZZn::IsInt() const
{
    if (start==NULL) return TRUE;
    if (one_term() && offset==0) return TRUE;
    return FALSE;
}

//
// return TRUE if zero or one term only in PS
//

BOOL Ps_ZZn::one_term() const
{
    int t=0;
    term_ps_zzn *ptr=start;
    if (start==NULL) return TRUE;
    while (ptr!=NULL)
    {
        if (!ptr->an.iszero()) t++;
        if (t>1) return FALSE;
        ptr=ptr->next;
    }
    return TRUE;
}

//
// add a term to a PS
//

term_ps_zzn* Ps_ZZn::addterm(const ZZn& a,int power,term_ps_zzn* pos)
{
    term_ps_zzn* newone;  
    term_ps_zzn* ptr;
    term_ps_zzn *t,*iptr;
    int dc,pw;
    ptr=start;
    iptr=NULL;

//
// intelligently determine the most compressed form to use
// for example if coefficient a=1 always, and power = -7 -5 -3 -1 1 3....
// then set pwr=2, offset=7  and PS = 1 + x + x^2
//
    pw=power+offset;
    if (one_term() && pw!=0)
    { // when PS has only one term, pwr is undefined
        if (pw<0)
            pwr=-pw;
        else pwr=pw; 
    }   

    dc=igcd(pw,pwr);

    if (dc != pwr) decompress(pwr/dc);
    power=pw/pwr;
// quick scan through to detect if term exists already
// and to find insertion point
   if (pos!=NULL) ptr=pos;
    while (ptr!=NULL) 
    { 
        if (ptr->n==power)
        {
            ptr->an+=a;

            if (ptr->an.iszero()) 
            { // delete term
                if (ptr==start)
                { // delete first one
                    start=ptr->next;
                    delete ptr;
                    norm();
                    return start;
                }
                iptr=ptr;
                ptr=start;
                while (ptr->next!=iptr)ptr=ptr->next;
                ptr->next=iptr->next;
                delete iptr;
                return ptr;
            }
            return ptr;
        }
        if (ptr->n<power) iptr=ptr;   // determines order
        else break;
        ptr=ptr->next;
    }
    newone=new term_ps_zzn;
    newone->next=NULL;
    newone->an=a;
    newone->n=power;
    pos=newone;
    if (start==NULL)
    {
        start=newone;
        norm();
        return pos;
    }

// insert at the start

    if (iptr==NULL)
    { 
        t=start;
        start=newone;
        newone->next=t;
        norm();
        return pos;
    }

// insert new term

    t=iptr->next;
    iptr->next=newone;
    newone->next=t;
    return pos;    
}

//
// Destructor
//

Ps_ZZn::~Ps_ZZn()
{
    term_ps_zzn *nx;
    while (start!=NULL)
    {
        nx=start->next;
        delete start;
        start=nx;
    }
}

//
// get coefficient of actual power
//

ZZn Ps_ZZn::coeff(int power) const
{
    ZZn c=0;
    term_ps_zzn *ptr=start;
    if ((power+offset)%pwr != 0) return c;  // no such term
    power=(power+offset)/pwr;

    while (ptr!=NULL)
    {
        if (ptr->n==power)
        {
            c=ptr->an;
            return c;
        }
        ptr=ptr->next;
    }
    return c;
}

//
// get coefficient of "Internal" power
//

ZZn Ps_ZZn::cf(int power) const
{
    ZZn c=0;
    term_ps_zzn *ptr=start;

    while (ptr!=NULL)
    {
        if (ptr->n==power)
        {
            c=ptr->an;
            return c;
        }
        ptr=ptr->next;
    }
    return c;
}

//
// Zeroise PS and reclaim space
//

void Ps_ZZn::clear()
{
    term_ps_zzn *ptr;
    while (start!=NULL)
    {
        ptr=start->next;
        delete start;
        start=ptr;
    }
    offset=0;
    pwr=1;
}

// Note: real power = internal power * pwr - offset 

Ps_ZZn& Ps_ZZn::operator+=(const Ps_ZZn& p)
{
    term_ps_zzn *ptr=p.start;
    term_ps_zzn *pos=NULL;
    int pw;   
    while (ptr!=NULL)
    {
        pw=ptr->n*p.pwr-p.offset;   // convert compressed to real
        if (pw>=psN) break;
        pos=addterm(ptr->an,pw,pos);
        ptr=ptr->next;
    }
    return *this;
}

Ps_ZZn operator-(const Ps_ZZn& p)
{
    Ps_ZZn r=p;
    term_ps_zzn *ptr=r.start;
    while (ptr!=NULL)
    {
        ptr->an=(-ptr->an);
        ptr=ptr->next;
    }
    return r;
}

Ps_ZZn& Ps_ZZn::operator-=(const Ps_ZZn& p)
{
    term_ps_zzn *ptr=p.start;
    term_ps_zzn *pos=NULL;
    int pw;

    while (ptr!=NULL)
    {
        pw=ptr->n*p.pwr-p.offset;
        if (pw>=psN) break;
        pos=addterm(-(ptr->an),pw,pos);
        ptr=ptr->next;
    }
    return *this;
}

Ps_ZZn operator+(const Ps_ZZn& a,const Ps_ZZn& b)
{
    Ps_ZZn sum=a;
    sum+=b;
    return sum;
}

Ps_ZZn operator-(const Ps_ZZn& a,const Ps_ZZn& b)
{
    Ps_ZZn sum=a;
    sum-=b;
    return sum;
}

//
// In situ multiplication. Coeeficients in multiplicand
// are directly overwritten. Saves a lot of copying.
//

Ps_ZZn& Ps_ZZn::operator*=(Ps_ZZn& b)
{
    term_ps_zzn *ptr,*bptr;
    int g,d,deg,ka,kb;
    if (IsInt())
    {
        if (start!=NULL) *this = start->an*b;
        return *this;
    }
    if (b.IsInt())
    {
        if (b.start==NULL) clear();
        else *this *=(b.start->an);
        return *this;
    }
    g=igcd(pwr,b.pwr);
    deg=psN/g;


#ifdef FFT
    if (deg>=FFT_BREAK_EVEN)
    {
        big *A,*B;
        A=(big *)mr_alloc(deg,sizeof(big));
        B=(big *)mr_alloc(deg,sizeof(big));
        ka=pwr/g;
        decompress(ka);
        pad();
        ptr=start;
        while (ptr!=NULL)
        {
            d=ptr->n;
            if (d>=deg) break;
            A[d]=getbig(ptr->an);
            ptr=ptr->next;
        }
        kb=b.pwr/g;
        b.decompress(kb);
        bptr=b.start;
        while (bptr!=NULL)
        {
            d=bptr->n;
            if (d>=deg) break;
            B[d]=getbig(bptr->an);
            bptr=bptr->next;
       } 
       mr_ps_zzn_mul(deg,A,B,A);
       mr_free(B); mr_free(A);
       b.compress(kb);
    }
    else
    {
#endif
        *this=*this*b;
        return *this;
#ifdef FFT
    }
#endif
    norm();
    offset+=b.offset;
    return *this;
}

Ps_ZZn operator*(Ps_ZZn& a,Ps_ZZn& b)
{
    Ps_ZZn prod;
    ZZn t;
    term_ps_zzn *aptr,*bptr,*pos;
    int ka,kb,i,pa,pb,d,deg,g;

    if (a.IsInt())
    {
        if (a.start==NULL) return prod;
        else return (a.start->an*b);
    }

    if (b.IsInt())
    {
        if (b.start==NULL) return prod;
        else return (b.start->an*a);
    }

    g=igcd(a.pwr,b.pwr);

    deg=psN/g;

#ifdef FFT
    if (deg>=FFT_BREAK_EVEN)
    { // use fast methods
        big *A,*B,*C;
        A=(big *)mr_alloc(deg,sizeof(big));
        B=(big *)mr_alloc(deg,sizeof(big));
        C=(big *)mr_alloc(deg,sizeof(big));
        char *memc=(char *)memalloc(deg);
        for (i=0;i<deg;i++) C[i]=mirvar_mem(memc,i);
       
        ka=a.pwr/g;
        a.decompress(ka);

        aptr=a.start;
        while (aptr!=NULL)
        {
            d=aptr->n;
            if (d >= deg) break;
            A[d]=getbig(aptr->an);
            aptr=aptr->next;
        }
        kb=b.pwr/g;
        b.decompress(kb);
        bptr=b.start;
        while (bptr!=NULL)
        {
            d=bptr->n;
            if (d >= deg) break;
            B[d]=getbig(bptr->an);
            bptr=bptr->next;
        }
        mr_ps_zzn_mul(deg,A,B,C);
        pos=NULL;
        for (d=0;d<deg;d++)
        {
            t=C[d];
            if (t.iszero()) continue;
            pos=prod.addterm(t,d*g,pos);
        }
        memkill(memc,deg);
        a.compress(ka); b.compress(kb);
        mr_free(C); mr_free(B); mr_free(A); 
    }
    else
    {
#endif
        bptr=b.start;

        while (bptr!=NULL)
        {
            aptr=a.start;
            pb=bptr->n*b.pwr-b.offset;
            pos=NULL;
            while (aptr!=NULL)
            {
                pa=aptr->n*a.pwr-a.offset;
                if (pb+pa>=psN) break;
                pos=prod.addterm(aptr->an*bptr->an,pa+pb,pos);
                aptr=aptr->next;
            }
            bptr=bptr->next;
        }
#ifdef FFT
    }
#endif

    if (prod.start!=NULL) prod.offset=a.offset+b.offset;
    return prod;
}

Ps_ZZn operator/(const ZZn& num,const Ps_ZZn& b)
{
    Ps_ZZn quo;
    term_ps_zzn *pos=NULL;
    int pw=b.pwr;
    ZZn w,v0=b.cf(0);

    for (int n=0;n<psN/pw;n++)
    {
        if ((n*pw+b.offset)>=psN) break;
        if (n==0) w=num;
        else w=0;
        for (int k=0;k<n;k++)
            w-=quo.cf(k)*b.cf(n-k);
        pos=quo.addterm(w/v0,n,pos); 
    }

    quo.pwr=pw;
    quo.offset=(-b.offset);
    return quo;
}

Ps_ZZn operator/(Ps_ZZn& a,Ps_ZZn& b)
{
    Ps_ZZn quo;
    term_ps_zzn *pos=NULL;
    int ka,kb,pw;

    ka=a.pwr;
    kb=b.pwr;

    if (ka!=kb)
    {
        pw=1;
        a.decompress(a.pwr);
        b.decompress(b.pwr);
    }
    else pw=ka;

    ZZn w,v0=b.cf(0);

    for (int n=0;n<psN/pw;n++)
    {
        if (n*pw+b.offset-a.offset>=psN) break;
        w=a.cf(n);
        for (int k=0;k<n;k++)
            w-=quo.cf(k)*b.cf(n-k);
        pos=quo.addterm(w/v0,n,pos); 
    }
    if (ka!=kb)
    {
       a.compress(ka); 
       b.compress(kb);
    }
    else quo.pwr=pw;

    quo.offset=a.offset-b.offset;
    return  quo;
}

Ps_ZZn& Ps_ZZn::operator/=(const ZZn& x)
{
    term_ps_zzn *ptr=start;
    while (ptr!=NULL)
    {
        ptr->an/=x;
        ptr=ptr->next;
    }
    return *this;
}

Ps_ZZn& Ps_ZZn::operator/=(int m)
{
    term_ps_zzn *ptr=start;
    while (ptr!=NULL)
    {
        ptr->an/=m;
        ptr=ptr->next;
    }
    return *this;
}

Ps_ZZn operator/(const Ps_ZZn& a,const ZZn& b)
{
    Ps_ZZn quo;
    quo=a;
    quo/=b;
    return quo;
}

Ps_ZZn operator/(const Ps_ZZn& a,int m)
{
    Ps_ZZn quo;
    quo=a;
    quo/=m;
    return quo;
}


Ps_ZZn& Ps_ZZn::operator=(int m)
{
    clear();
    if (m!=0) addterm((ZZn)m,0);
    return *this;
}

Ps_ZZn& Ps_ZZn::operator=(const Ps_ZZn& p)
{
    term_ps_zzn *ptr;
    term_ps_zzn *pos=NULL;
    clear();
    int pw;

    pwr=p.pwr;
    offset=p.offset;
    ptr=p.start;
    while (ptr!=NULL)
    {
        pw=ptr->n*p.pwr-p.offset;
        if (pw>=psN) break;
        pos=addterm(ptr->an,pw,pos);
        ptr=ptr->next;
    }

    return *this;
}

Ps_ZZn& Ps_ZZn::operator*=(const ZZn& x)
{
    term_ps_zzn *ptr=start;
    if (x.iszero()) 
    {
        clear();
        return *this;
    }
    while (ptr!=NULL)
    {
        ptr->an*=x;
        ptr=ptr->next;
    }
    return *this;
}

Ps_ZZn operator*(const ZZn& z,const Ps_ZZn &p)
{
    Ps_ZZn r=p;
    r*=z;
    return r;
}

Ps_ZZn operator*(const Ps_ZZn &p,const ZZn& z)
{
    Ps_ZZn r=p;
    r*=z;
    return r;
}

Ps_ZZn pow(Ps_ZZn &a,int k)
{
    Ps_ZZn res;
    int w,e;

    if (k==0) 
    {
        res=1;
        return res;
    }
    res=a;
    if (k==1) return res;

    e=k; w=1;
    while (w<=e) w<<=1;
    w>>=1; 
    e-=w; w/=2;
    while (w>0)
    {
        res*=res;
        if (e>=w)
        {
            e-=w;
            res*=a;
        }
        w/=2;
    }
    res.offset*=k;
    
    return res;
}


Ps_ZZn power(const Ps_ZZn &a,int e)
{ // return f(a^e), e is +ve
    Ps_ZZn res;
    term_ps_zzn *ptr;
    term_ps_zzn *pos=NULL;

    ptr=a.start;
    while (ptr!=NULL)
    {
        if ((ptr->n*a.pwr)*e < psN)
        {
            pos=res.addterm(ptr->an,ptr->n,pos);
            ptr=ptr->next;
        }
        else break;
    }

    res.pwr=e*a.pwr;
    res.offset=e*a.offset;

    return res;
}

ostream& operator<<(ostream& s,const Ps_ZZn& p)
{
    BOOL first=TRUE;
    ZZn a;
    term_ps_zzn *ptr=p.start;
    int pw;

    if (ptr==NULL)
    {
        s << "0";
        return s;
    }

    while (ptr!=NULL)
    {
        a=ptr->an;
        if (a.iszero()) 
        {
            ptr=ptr->next;
            continue;
        }
        if ((Big)a < get_modulus()/2) 
        {
            a=(-a);
            s << " - ";
        }
        else if (!first) s << " + ";

        first=FALSE;
        pw=ptr->n*p.pwr-p.offset;
        if (pw==0)
        {
            s << a;
            ptr=ptr->next;
            continue;
        } 

        if (a==(ZZn)1) s << "x";
        else      s << a << "*x";
    
        if (pw!=1) s << "^" << pw;
        ptr=ptr->next;
    }
    return s;
} 

