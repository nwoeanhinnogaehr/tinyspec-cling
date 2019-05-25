vector<int> feed[2];
vector<cplx> cfeed[2];

fill(feed[0].begin(),feed[0].end(),0); fill(feed[1].begin(),feed[1].end(),0);

extern "C" void synth_main(cplx*buf[2],int n,double t){ } //hush

extern "C" void synth_main(cplx*buf[2],int n,double t){
    for (int c = 0; c < 2; c++) {
        feed[c].resize(n,0);
        for (int i = 1; i < n; i++) {
            int r = int(t*16)*n+i+c+feed[c][(i+1)%n];
            int l = feed[c][i];
            feed[c][i] ^= (l^r/127^r/13);
            if (int mod = (r/16^r/256^r/64))
                feed[c][i] %= mod;
            buf[c][i] = (exp(cplx(0.0,M_PI/64.0*feed[c][i])))/pow(i+1, 0.7)/2.0;
        }
    }
    set_next_size(1<<10);
}

extern "C" void synth_main(cplx*buf[2],int n,double t){
    for (int c = 0; c < 2; c++) {
        feed[c].resize(n,0);
        for (int i = 1; i < n; i++) {
            int r = int(t*16)*n+i+c+feed[c][(i+1)%n];
            feed[c][i] ^= r/64|r/32|r/1024;
            feed[c][i] &= r/3&r/(int(t*8)%32+1);
            buf[c][i] = (exp(cplx(0.0,pow(i,1.2)*M_PI/32.0*feed[c][i]/(i+1))))/pow(i+1, 0.8)/4.0;
        }
    }
    set_next_size(1<<16);
}

extern "C" void synth_main(cplx*buf[2],int n,double t){
    for (int c = 0; c < 2; c++) {
        feed[c].resize(n,0);
        for (int i = 1; i < n; i++) {
            int r = int(t*16)*n+i;
            feed[c][i] &= feed[1-c][(i-1)%n];
            feed[c][i] += r/128|r/32&r/1024|r/16384;
            feed[c][i] &= 1+abs(r/7&r/5&r/11);
            buf[c][i] = (exp(cplx(0.0,pow(i,1.01)*M_PI/32.0*feed[c][i]/(i+1))))/pow(i+1, 0.8)/4.0;
        }
    }
    set_next_size(1<<10);
}

extern "C" void synth_main(cplx*buf[2],int n,double t){
    for (int c = 0; c < 2; c++) {
        for (int i = 1; i < n; i++) {
            int r = int(t*32)*n+i;
            feed[c][i] += r/(abs(feed[c][i])+1)^r/3;
            feed[c][i] &= r/7^r/11^r/17;
            buf[c][i]=sin(2.0*M_PI/32.*(c+((int)pow(r,1.1))|r/768|r/256|r/512|r/1280|feed[c][i]))/pow(i+1, 0.9)/2.0;
        }
    }
    set_next_size(1<<11);
}

extern "C" void synth_main(cplx*buf[2],int n,double t){
    for (int c = 0; c < 2; c++) {
        feed[c].resize(n,0);
        for (int i = 1; i < n; i++) {
            int r = int(t*12)*n+i+c;
            feed[c][i] &= r/2^r/3^r/(i+2);
            feed[c][i] += r/512&r/128^r/1024^r/16384;
            feed[c][i] /= abs(feed[c][(i+feed[c][i])%n])+1;
            buf[c][i] = (exp(cplx(0.0,pow(i,0.01)*M_PI/32.0*feed[c][i]/(i+1))))/pow(i+1, 0.7)/4.0;
        }
    }
    set_next_size(1<<12);
}

extern "C" void synth_main(cplx*buf[2],int n,double t){
    for (int c = 0; c < 2; c++) {
        cfeed[c].resize(n,0);
        for (int i = 1; i < n; i++) {
            int r = int(t*1)*n+i;
            buf[c][i]=exp(cplx(0.0,(cfeed[c][i].real()+2.0*M_PI/1024.*(c+((int)pow(cfeed[c][(i+1)%n].real()+r,1.1))^r/(int(t*8)%256+1)^r/5^r/7^r/511^r/1025))))/pow(i+1, 0.9)/2.0;
            cfeed[c][i]+=buf[c][i].real()*i;
        }
    }
    set_next_size(1<<12);
}
