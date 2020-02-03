N = 100;

x = zeros(N, 1);
x(1) = 1.0;
for n = 2:N
  if x(n - 1) > 0.1
    x(n) = 0.95 * x(n - 1);
  endif
  if x(n) < 0.1
    x(n) = 0.0;
  endif
endfor

plot(x);
