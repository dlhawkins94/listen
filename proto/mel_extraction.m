K = 24;
k = 0:K;
fstart = 64;
fs = 16000;
fftN = 256;

fc = melinv(mel(fstart) + k * (mel(fs / 2) - mel(fstart)) / K);
cbin = round(fftN * fc / fs);

n = 0:255;
# x = cos(1 * 2*pi*n/256);
x = randn(256,1);
X = fft(x, fftN);
fbank = zeros(K, 1);

for k = (1:(K-1)) + 1
  for i = cbin(k-1):cbin(k)
    fbank(k) = fbank(k) + X(i+1) * (i-cbin(k-1)+1) / (cbin(k)-cbin(k-1)+1);
  endfor
  for i = (cbin(k) + 1):cbin(k+1)
    fbank(k) = fbank(k) + X(i+1) * (1-(i - cbin(k))/(cbin(k+1) - cbin(k) + 1));
  endfor
endfor
