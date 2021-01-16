function [ A ] = wl( sf, np, r, f, Th)
%greedy: analytic formula for wear leving amplification
    gc = greedy_hc(sf, np, r, f);
    A = (2.0 * (1-sf) * ((r/(1-sf)) - f)) / ((Th+1) * ((1/(1-sf)) - f)) * gc;
end
