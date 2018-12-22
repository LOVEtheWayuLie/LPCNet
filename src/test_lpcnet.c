/* Copyright (c) 2018 Mozilla */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <math.h>
#include <stdio.h>
#include "arch.h"
#include "lpcnet.h"
#include "freq.h"

#include "ceps_codebooks.c"

int vq_quantize(const float *codebook, int nb_entries, const float *x, int ndim, float *dist)
{
  int i, j;
  float min_dist = 1e15;
  int nearest = 0;
  
  for (i=0;i<nb_entries;i++)
  {
    float dist=0;
    for (j=0;j<ndim;j++)
      dist += (x[j]-codebook[i*ndim+j])*(x[j]-codebook[i*ndim+j]);
    if (dist<min_dist)
    {
      min_dist = dist;
      nearest = i;
    }
  }
  if (dist)
    *dist = min_dist;
  return nearest;
}

int quantize(float *x, float *mem)
{
    int i;
    int id, id2;
    float ref[NB_BANDS];
    RNN_COPY(ref, x, NB_BANDS);
    for (i=0;i<NB_BANDS;i++) {
        x[i] -= 0.75f*mem[i];
    }
    id = vq_quantize(ceps_codebook1, 1024, x, NB_BANDS, NULL);
    for (i=0;i<NB_BANDS;i++) {
        x[i] -= ceps_codebook1[id*NB_BANDS + i];
    }
    id2 = vq_quantize(ceps_codebook2, 1024, x, NB_BANDS, NULL);
    for (i=0;i<NB_BANDS;i++) {
        x[i] = ceps_codebook2[id2*NB_BANDS + i];
    }
    for (i=0;i<NB_BANDS;i++) {
        x[i] += ceps_codebook1[id*NB_BANDS + i];
    }
    for (i=0;i<NB_BANDS;i++) {
        x[i] += 0.75f*mem[i];
        mem[i] = x[i];
    }
    if (0) {
        float err = 0;
        for (i=0;i<NB_BANDS;i++) {
            err += (x[i]-ref[i])*(x[i]-ref[i]);
        }
        printf("%f\n", sqrt(err/NB_BANDS));
    }
    
    return id;
}


int main(int argc, char **argv) {
    FILE *fin, *fout;
    LPCNetState *net;
    float vq_mem[NB_BANDS] = {0};
    net = lpcnet_create();
    if (argc != 3)
    {
        fprintf(stderr, "usage: test_lpcnet <features.f32> <output.pcm>\n");
        return 0;
    }
    fin = fopen(argv[1], "rb");
    if (fin == NULL) {
	fprintf(stderr, "Can't open %s\n", argv[1]);
	exit(1);
    }

    fout = fopen(argv[2], "wb");
    if (fout == NULL) {
	fprintf(stderr, "Can't open %s\n", argv[2]);
	exit(1);
    }

    while (1) {
        float in_features[NB_TOTAL_FEATURES];
        float features[NB_FEATURES];
        short pcm[FRAME_SIZE];
        fread(in_features, sizeof(features[0]), NB_TOTAL_FEATURES, fin);
        if (feof(fin)) break;
        RNN_COPY(features, in_features, NB_FEATURES);
        RNN_CLEAR(&features[18], 18);
        quantize(features, vq_mem);
        features[37] = (1.f/7)*floor(.5 + 7*features[37]); 
        lpcnet_synthesize(net, pcm, features, FRAME_SIZE);
        fwrite(pcm, sizeof(pcm[0]), FRAME_SIZE, fout);
    }
    fclose(fin);
    fclose(fout);
    lpcnet_destroy(net);
    return 0;
}
