# https://github.com/JonCooperWorks/httpfuzz

# ../fuzzdb/attack/all-attacks/all-attacks-unix.txt
# ../fuzzdb/attack/format-strings/format-strings.txt

httpfuzz \
   --wordlist ../fuzzdb/attack/format-strings/format-strings.txt \
   --seed-request fuzz/POST.request \
   --target-header User-Agent \
   --target-header Host \
   --delay-ms 50 \
   --target-header Pragma \
   --skip-cert-verify \
   --target-param fuzz \
   --dirbuster
