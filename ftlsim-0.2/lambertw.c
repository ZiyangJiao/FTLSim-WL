/*
 * lambertw.c - translation of Nicol Schraudolph's OctaveForge
 *              implementation of the Lambert W function into C
 * Peter Desnoyers, 2011
 *
 * Copyright 2012 Peter Desnoyers
 * This file is part of ftlsim.
 * ftlsim is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version. 
 *
 * ftlsim is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details. 
 * You should have received a copy of the GNU General Public License
 * along with ftlsim. If not, see http://www.gnu.org/licenses/. 
 */

/*
%% usage: W(z) or W(n,z)
%%
%% Compute the Lambert W function of z.  This function satisfies
%% W(z).*exp(W(z)) = z, and can thus be used to express solutions
%% of transcendental equations involving exponentials or logarithms.
%%
%% n must be integer, and specifies the branch of W to be computed;
%% W(z) is a shorthand for W(0,z), the principal branch.  Branches
%% 0 and -1 are the only ones that can take on non-complex values.
%%
%% If either n or z are non-scalar, the function is mapped to each
%% element; both may be non-scalar provided their dimensions agree.
%%
%% This implementation should return values within 2.5*eps of its
%% counterpart in Maple V, release 3 or later.  Please report any
%% discrepancies to the author, Nici Schraudolph <schraudo@inf.ethz.ch>.
%%
%% For further details, see:
%%
%% Corless, Gonnet, Hare, Jeffrey, and Knuth (1996), "On the Lambert
%% W Function", Advances in Computational Mathematics 5(4):329-359.

%% Author:   Nicol N. Schraudolph <schraudo@inf.ethz.ch>
%% Version:  1.0
%% Created:  07 Aug 1998
%% Keywords: Lambert W Omega special transcendental function

%% Copyright (C) 1998 by Nicol N. Schraudolph
%%
%% This program is free software; you can redistribute and/or
%% modify it under the terms of the GNU General Public
%% License as published by the Free Software Foundation;
%% either version 2, or (at your option) any later version.
%%
%% This program is distributed in the hope that it will be
%% useful, but WITHOUT ANY WARRANTY; without even the implied
%% warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
%% PURPOSE.  See the GNU General Public License for more
%% details.
%%
%% You should have received a copy of the GNU General Public
%% License along with Octave; see the file COPYING.  If not,
%% write to the Free Software Foundation, 59 Temple Place -
%% Suite 330, Boston, MA 02111-1307, USA.
*/

#include <stdio.h>
#include <math.h>
#include <complex.h>

static double lambertw_2(int b, double z)
{
    double eps = 2e-16;

    /* %% series expansion about -1/e */
    /* % */
    /* % p = (1 - 2*abs(b)).*sqrt(2*e*z + 2); */
    /* % w = (11/72)*p; */
    /* % w = (w - 1/3).*p; */
    /* % w = (w + 1).*p - 1 */
    /* % */
    /* % first-order version suffices: */
    /* % */
    /* w = (1 - 2*abs(b)).*sqrt(2*e*z + 2) - 1; */
    /* note b=0 */
    
    complex double w = (1-2*abs(b)) * sqrt(2*M_E*z + 2) - 1;

    /* %% asymptotic expansion at 0 and Inf */
    /* % */
    /* v = log(z + ~(z | b)) + 2*pi*I*b; */
    /* v = v - log(v + ~v); */
    complex double v = clog(z + !z) + 2*M_PI*I*b;
    v = v - clog(v + !v);

    /* %% choose strategy for initial guess */
    /* % */
    /* c = abs(z + 1/e); */
    /* c = (c > 1.45 - 1.1*abs(b)); */
    /* c = c | (b.*imag(z) > 0) | (~imag(z) & (b == 1)); */
    /* w = (1 - c).*w + c.*v; */

    double c = fabs(z + 1/M_E);
    c = (c > 1.45 - 1.1*abs(b));
    w = (1 - c)*w + c*v;

    int i;
    for (i = 0; i < 10; i++) {
        complex double p = exp(w);
        complex double t = w*p - z;
        double f = (w != -1);
        t = f*t/(p*(w + f) - 0.5*(w + 2.0)*t/(w + f));
        w = w - t;

        if (fabs(creal(t)) < (2.48*eps)*(1.0 + fabs(creal(w)))
            && fabs(cimag(t)) < (2.48*eps)*(1.0 + fabs(cimag(w))))
            return creal(w);
    }
//    printf("warning: iteration limit reached, result of W may be inaccurate\n");
    return creal(w);
}

double lambertw(double z)
{
    return lambertw_2(0,z);
}

#if 0
int main()
{
    double i;
    for (i = -0.3; i < 1; i += 0.01)
	printf("%.2f %.30f\n", i, lambertw(i));
    return 0;
}
#endif
