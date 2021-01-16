for Th = [3, 5, 8, 10]
    for sf = [0.05, 0.07, 0.11, 0.2]
        for r = [0.9, 0.8, 0.7]
            for f = [0.1, 0.2, 0.3]
                if r + f == 1
                    res = wl(sf, 64, r, f, Th);
                    fprintf('Th = %d sf = %.2f r = %.2f f = %.2f res = %.4f\n', Th, sf, r, f, res);
                end
            end
        end
    end
end
