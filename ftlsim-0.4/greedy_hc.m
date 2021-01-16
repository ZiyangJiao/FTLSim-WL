function [ A ] = greedy_hc( sf, np, r, f )
%greedy: analytic formula for greedy write amplification
    alpha = 1 / (1 - sf);
    k = 1 + 1/(2*np);
    A = lru_hc(k * alpha, r, f) / k;
end

