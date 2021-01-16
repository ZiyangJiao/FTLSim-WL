function [ A ] = lru_hc( alpha, r, f )
%lru_hc: Numeric solution of mixed LRU performance for hot/cold traffic
%   see SYSTOR '12 Eq. 14
    hot_term  = @(A_) r / (exp((r/f)*(alpha/A_))-1);
    cold_term = @(A_) (1-r) / (exp(((1-r)/(1-f))*(alpha/A_))-1);
    A = fzero(@(A_) 1 + hot_term(A_) + cold_term(A_) - A_, [1.00001, 10000]);
end

% Copyright 2012 Peter Desnoyers
