// Kyutech Arduino Scope Prototype  v0.72                     Mar 20, 2016
//
//    (C) 2012-2016 M.Kurata Kyushu Institute of Technology
//
//    for Arduinos with a 5V-16MHz ATmega328.
//
//    use with "kit_scope.pde", a Proce55ing GUI sketch.
//
//    You don't need to worry about this warning message produced by the IDE.
//    "Low memory available, stability problems may occur."
//    コンパイル時、下記メッセージが表示されますが、問題ありません。
//    "スケッチが使用できるメモリが少なくなっています。動作が不安定になる可能性があります。"
//
//
//    Pin usage
//    
//    A0  trigger level voltage input    (connected to D6)
//    A1  oscilloscope probe ch1
//    A2  oscilloscope probe ch2
//    A3  oscilloscope probe ext  trigger
//    A4  reserved
//    A5  reserved
//    A6  reserved
//    A7  reserved
//    
//    D0  uart-rx
//    D1  uart-tx
//    D2  reserved
//    D3  calibration pulse wave output
//    D4  reserved
//    D5  pwm output for generating trigger level voltage
//    D6  analog comparator input (trigger level)
//    D7  reserved
//    D8  reserved
//    D9  reserved
//    D10 reserved
//    D11 reserved
//    D12 reserved
//    D13 LED output
//
//    different usage for dks2014 board.
//    A4  fgen-sync
//    D8  CH1 mode input  0..[0-10V]  1..[-5..5V]  (pull-up needed)
//    D9  CH2 mode input  0..[0-10V]  1..[-5..5V]  (pull-up needed)



const byte cfg_cupgain = 0; // the usage definition of the pins D7..D12
                            // 0: input  1:input(pulled-up)  2:output

word  oscversion = 0x0001;
word  oscvbg;              // band gap voltage in 10bits  0 means a failure.
word  oscconfig;           // bit0 -> if trigger voltage is 0:uncontrollable
                           //                               1:controllable
                           // bit1 -> optional-fgen is      0:not attached
                           //                               1:attached
                           // bit2 -> cupgain is            0:input
                           //                               1:output
byte  oscspeed   = 0;      // 0..3:real 4..7:equiv 8:roll
byte  oscinput   = 0;      // input signal selection  0:CH1 1:CH2 2:DUAL

byte  osctrig    = 0;      // trigger bit012-> 000:CH1 001:CH2 010:EXT
                           //                  011:built-in-pulse
                           //                  100:optional-fgen
                           //         bit4  -> 0:rising 1:falling
                           //         bit5  -> 0:auto   1:normal
word  osctdly    = 100;    // time of delayed trigger  100..30000 usec
byte  osctvolt;            // trigger level voltage (measured by adc) 0..255
byte  osctduty;            // trigger level duty  0..255

byte  osccupgain = 0;      // bit0  -> CH1 coupling        0:dc  1:ac
                           // bit1  -> CH2 coupling        0:dc  1:ac
                           // bit23 -> CH1 gain-selection  0,1,2,3
                           // bit45 -> CH2 gain-selection  0,1,2,3

long  oscofreq   = 1000;   // 31 .. 2000000Hz
byte  oscoduty   =   50;   // 0..100%

byte  fgen;                // 0..3: fgen-dipsw  255:no-fgen


#define TXBSZ 1100
#define RXBSZ  256 //  this must be 256.
#define RMBSZ  256 //  this must be 256.  for rollmode

int  txn, txr;
byte txcrc, rxn;
byte rmw, rmr, rmon;

byte txbuf[TXBSZ];
byte rxbuf[RXBSZ];

// since no huge packets are sent in rollmode,
// the last 256 bytes of the txbuf[] can be used
// as another buffer during the rollmode.
//
// byte rmbuf[RMBSZ];
#define rmbuf (&txbuf[TXBSZ - RMBSZ])

static const unsigned char crctbl[256] = {
    0x00, 0x85, 0x8F, 0x0A, 0x9B, 0x1E, 0x14, 0x91,
    0xB3, 0x36, 0x3C, 0xB9, 0x28, 0xAD, 0xA7, 0x22,
    0xE3, 0x66, 0x6C, 0xE9, 0x78, 0xFD, 0xF7, 0x72,
    0x50, 0xD5, 0xDF, 0x5A, 0xCB, 0x4E, 0x44, 0xC1,
    0x43, 0xC6, 0xCC, 0x49, 0xD8, 0x5D, 0x57, 0xD2,
    0xF0, 0x75, 0x7F, 0xFA, 0x6B, 0xEE, 0xE4, 0x61,
    0xA0, 0x25, 0x2F, 0xAA, 0x3B, 0xBE, 0xB4, 0x31,
    0x13, 0x96, 0x9C, 0x19, 0x88, 0x0D, 0x07, 0x82,

    0x86, 0x03, 0x09, 0x8C, 0x1D, 0x98, 0x92, 0x17,
    0x35, 0xB0, 0xBA, 0x3F, 0xAE, 0x2B, 0x21, 0xA4,
    0x65, 0xE0, 0xEA, 0x6F, 0xFE, 0x7B, 0x71, 0xF4,
    0xD6, 0x53, 0x59, 0xDC, 0x4D, 0xC8, 0xC2, 0x47,
    0xC5, 0x40, 0x4A, 0xCF, 0x5E, 0xDB, 0xD1, 0x54,
    0x76, 0xF3, 0xF9, 0x7C, 0xED, 0x68, 0x62, 0xE7,
    0x26, 0xA3, 0xA9, 0x2C, 0xBD, 0x38, 0x32, 0xB7,
    0x95, 0x10, 0x1A, 0x9F, 0x0E, 0x8B, 0x81, 0x04,

    0x89, 0x0C, 0x06, 0x83, 0x12, 0x97, 0x9D, 0x18,
    0x3A, 0xBF, 0xB5, 0x30, 0xA1, 0x24, 0x2E, 0xAB,
    0x6A, 0xEF, 0xE5, 0x60, 0xF1, 0x74, 0x7E, 0xFB,
    0xD9, 0x5C, 0x56, 0xD3, 0x42, 0xC7, 0xCD, 0x48,
    0xCA, 0x4F, 0x45, 0xC0, 0x51, 0xD4, 0xDE, 0x5B,
    0x79, 0xFC, 0xF6, 0x73, 0xE2, 0x67, 0x6D, 0xE8,
    0x29, 0xAC, 0xA6, 0x23, 0xB2, 0x37, 0x3D, 0xB8,
    0x9A, 0x1F, 0x15, 0x90, 0x01, 0x84, 0x8E, 0x0B,

    0x0F, 0x8A, 0x80, 0x05, 0x94, 0x11, 0x1B, 0x9E,
    0xBC, 0x39, 0x33, 0xB6, 0x27, 0xA2, 0xA8, 0x2D,
    0xEC, 0x69, 0x63, 0xE6, 0x77, 0xF2, 0xF8, 0x7D,
    0x5F, 0xDA, 0xD0, 0x55, 0xC4, 0x41, 0x4B, 0xCE,
    0x4C, 0xC9, 0xC3, 0x46, 0xD7, 0x52, 0x58, 0xDD,
    0xFF, 0x7A, 0x70, 0xF5, 0x64, 0xE1, 0xEB, 0x6E,
    0xAF, 0x2A, 0x20, 0xA5, 0x34, 0xB1, 0xBB, 0x3E,
    0x1C, 0x99, 0x93, 0x16, 0x87, 0x02, 0x08, 0x8D,
};


void initt0();
void rxinit(void);
void sett0(byte duty);
void sett2(long hz, byte duty);
void sysdown(int dly);
void txput0(byte ch);
void uartjob();
void wait0(word num, boolean uart);


void
rxinit(void)
{
    rxn = 0;
}


int
rxnum(void)
{
    return  rxn;
}


void
txinit(void)
{
    if (txn >= TXBSZ)
        sysdown(200);      // tx buffer overflow has been detected.
    txn = 0;
    txr = TXBSZ;
}

void
txgo(void)
{
    if (txr == TXBSZ)
        txr = 0;
}


boolean
txrunning()
{
    if (txn > txr)
        return  true;
    if (txr == TXBSZ)
        return  false;
    return  (UCSR0A & 0x40) ? false : true;
}


void
txfinish(boolean epilogue, boolean waircv __attribute__((unused)))
{
    byte    i;

    if (epilogue) {    // output epilogue mark
        for(i = 0; i < 4; i++) {
            txput0(0xa0);
            txput0(0xa0);
            txput0(0xa0);
            txput0(0xa0);
            uartjob();
        }
    }
    while(txrunning())
        uartjob();
    //if (waircv) {
    //    wait0(600, true);  // about 10ms wait
    //}
}


void
txput0(byte ch)
{
    // to reduce the cpu consumption,
    // venture to omit txn overflow check.
    // if programmed properly, such an overflow never occurs.
    // rxn(appears later) check is omitted as well.
    txbuf[txn++] = ch;
    txcrc = 0;
}


void
txput1(byte ch)
{
    txbuf[txn++] = ch;
    txcrc = crctbl[txcrc ^ ch];
}


void
txputcrc(boolean force_error)
{
    txput0((force_error) ? ++txcrc : txcrc);
}


void
rminit(boolean on)
{
    rmw = rmr = 0;
    rmon = (on) ? 1 : 0;
}


void
uartjob()
{
    byte    sts;

    sts = UCSR0A;
    if ((char)sts < 0)
        rxbuf[rxn++] = UDR0;
    // in case rxbuf[] overflow, no fatal situation happens.
    // because rxn is an 8 bit variable and rxbuf[] size is 256.

    if (rmon) {
        if (TIFR1 & 0x20)
            TIFR1  = 0x27;  // clear timer1 flags;
        if (ADCSRA & 0x10) {
            if (rmon == 1) {
                ICR1  = 100 - 1;      // 50us
                ADMUX = 0x62;
                rmon = 2;
                rmbuf[rmw++] = ADCH;  // CH1(A1pin) value
            }
            else if (rmon == 2) {
                ICR1  = 400 - 1;      // 200us
                ADMUX = 0x60;
                rmbuf[rmw++] = ADCH;  // CH2(A2pin) value
                rmon = 3;
            }
            else {
                ICR1  = 500 - 1;      // 250us
                ADMUX = 0x61;
                rmon = 1;
                osctvolt = ADCH;       // trigger level
            }
            ADCSRA = 0xb4;  // clear flags, 1MHz, adate on
            return; //in order to release cpu quickly
        }
    }

    if (txr < txn && (sts & 0x20)) {
        UCSR0A = (sts & 0xe3) | 0x40;
        UDR0 = txbuf[txr++];
    }
}


void
header(byte typ, byte reqid)
{
    static byte seq;

    txinit();
    uartjob();

    // prologue
    txput0(0xaa);
    txput0(0x55);
    txput0(0xa5);
    txput0(0x5a);

    uartjob();

    txput1(typ);
    if (typ == 3)
        txput1(seq++);  // rollmode
    else
        txput1(reqid);
    txput1(oscspeed);
    txput1(oscinput);
    uartjob();

    txput1(osctrig);
    txput1(osccupgain);
    txput1(osctdly >> 8);
    txput1(osctdly);
    uartjob();

    txput1(oscofreq >> 16);
    txput1(oscofreq >> 8);
    txput1(oscofreq);
    txput1(oscoduty);
    uartjob();
}


void
modereal()
{
    static const byte sitbl[4] = {20, 50, 100, 200}; // 20,50,100,200usec
    int    i, realnum;
    byte   vh, adch0, adch1, crcnt;
    word   ui1, sint1, sint2;

    rminit(false);

    realnum = 520;                                    // == 1040 >> 1
    sint1 = sint2 = ((word)sitbl[oscspeed & 3]) << 1;
    switch(oscinput) {
    default:
    case 0x00: adch0 = 0x61; adch1 = 0x61; break;
    case 0x01: adch0 = 0x62; adch1 = 0x62; break;
    case 0x02: adch0 = 0x61; adch1 = 0x62;
               sint1 = 40;  // 20usec
               sint2 = sint2 + sint2 - sint1;
               break;
    }

    uartjob();

    header(1, 0);
    // This data packet contines to MARC-A

    // reset and initialize timer1
    TCCR1B = 0x00; // stop, set normal mode
    TCCR1A = 0x00;
    TIMSK1 = 0x00; // no irq
    ICR1   = 0x0000;
    TCNT1  = 0x0000;
    TIFR1  = 0x27; // clear flags;

    // analog comparator setting
    // The D6 pin is the positive input.
    // The negative input is A1, A2, A3, or A4 pin.
    ACSR   = 0x94;  // analog comparator off
    DIDR1  = 0x03;  // disable the digital input function of D6 and D7.
    ADMUX  = 0x61 + (osctrig & 7); // select the negative input
    ADCSRA = 0x04;
    ADCSRB = 0x40;

    // start timer1 with pre=1/8
    // input capture noise canceler ON
    TCCR1B = (osctrig & 0x10) ? 0xc2 : 0x82;  // falling or rising edge
    ACSR   = 0x14; // capture-on, aco to caputure timer1
    TIFR1  = 0x27;

    // falling edge detection(rising edge for ICES1)
    // doesn't stabilize without a 20usec wait below.
    while(TCNT1 < 40)
        ;
    TIFR1 = 0x27;

    ui1 = (osctdly << 1);
    // wait until a trigger event happens
    while(true) {
        if (TIFR1 & 0x20) {
            // trigger event has happened.
            ui1 += ICR1;
            crcnt = 0; // to indicate that a trigger event has happened.
            break;
        }
        if (TCNT1 > 60000) {  // 30ms == 60000/2 us
            // 30ms has passed without detecting a trigger event.
            ui1 += TCNT1;
            crcnt = 1;
            break;
        }
        uartjob();
    }

    // trigger event has happened here or timeout.
    ACSR   = 0x94; // disable analog comparator
    ADCSRB = 0x00;
    ADCSRA = 0x84; // adc enable

    TCCR1B = 0x1a; // timer1 CTC-ICR1 mode pre1/8
    TCCR1A = 0x00; //             CTC mode;
    ICR1   = ui1;
    TIFR1  = 0x27; // clear flags

    ADMUX  = 0x60; // adc target is A0 pin to get trigger value;
    ADCSRB = 0x07; // timer1 capture event;
    ADCSRA = 0xf4; // adc auto trigger, force start 1st conversion

    // wait until the 1st conversion finishes. 
    while((ADCSRA & 0x10) == 0x00)
        uartjob();
    vh = ADCH;  // trigger level
    osctvolt = vh;

    ADMUX  = adch0;
    ADCSRA = 0xb4;   // clear flag, 1MHz, adate on

    // MARC-A  continued
    txput1(crcnt);  // 0:triggered  1:freerun
    txput1(vh);     // trigger level voltage
    txputcrc(false);
    txgo(); // start to trasmit a packet

    if (crcnt && (osctrig & 0x20) != 0)
        goto  ex;  // when no-trigger and normal/single

    sint1--;
    sint2--;
    crcnt = 0;
    for(i = 0; i < realnum; i++) {
        while(1) {
            if (TIFR1 & 0x20) {
                ICR1  = sint1;
                TIFR1 = 0x27; // clear timer1 flags;
            }
            if ((ADCSRA & 0x10) != 0x00)
                break;
            uartjob();
        }
        vh = ADCH;
        ADMUX = adch1;
        ADCSRA = 0xb4;   // clear flag, 1MHz, adate on
        txput1(vh);

        while(1) {
            if (TIFR1 & 0x20) {
                ICR1  = sint2;
                TIFR1 = 0x27; // clear timer1 flags;
            }
            if ((ADCSRA & 0x10) != 0x00)
                break;
            uartjob();
        }
        vh = ADCH;
        ADMUX = adch0;
        ADCSRA = 0xb4;   // clear flag, 1MHz, adate on
        txput1(vh);

        if (++crcnt >= 100) {
            crcnt = 0;
            txputcrc(false);
        }
    }
    if (crcnt == 20) {
        txputcrc(false);
    }
    else
        sysdown(1000);

ex:
    txfinish(true, true);
}


void
modeequiv()
{
    static const struct eqdic_s {
        byte   tkn;
        byte   tdif;
        int    rnum;
        word   wu;
    } eqdic[] = {
        {20,  2,  52,  4000},
        {10,  4, 104,  4000},
        { 4, 10, 260, 10000},
        { 2, 20, 520, 20000},
    };
    const struct eqdic_s   *eq;
    int    realnum, i;
    byte   at, crcnt, tokadif, toka, tokanum;
    byte   ch, chnum, vh, adch, adchT;
    word   ui1, waituntil, sinterval;

    rminit(false);

    eq = &eqdic[oscspeed & 3];
    tokanum   = eq->tkn;
    waituntil = eq->wu;
    realnum   = eq->rnum;
    tokadif   = eq->tdif;
    sinterval = 40;      // 20us

    uartjob();

    // ADMUX reg values
    switch(oscinput) {
    default:
    case 0x00: adch = 0x61; chnum = 1;  break;
    case 0x01: adch = 0x62; chnum = 1;  break;
    case 0x02: adch = 0x61; chnum = 2;
        tokanum >>= 1;
        tokadif <<= 1;
        break;
    }
    adchT = 0x61 + (osctrig & 7);

    header(2, 0);
    // This data packet contines to MARC-A

    sinterval--;
    crcnt = 0;
    at = 0;
    for(toka = 0; toka < tokanum; toka++) {
        for(ch = 0; ch < chnum; ch++) {
            // reset and initialize timer1
            TCCR1B = 0x00; // stop, set normal mode
            TCCR1A = 0x00;
            TIMSK1 = 0x00; // no irq
            ICR1   = 0x0000;
            TCNT1  = 0x0000;
            TIFR1  = 0x27; // clear flags;

            // analog comparator setting
            // The D6 pin is the positive input.
            // The negative input is A1, A2, A3, or A4 pin.
            ACSR   = 0x94;  // analog comparator off
            DIDR1  = 0x03;  // disable the digital input func of D6 and D7.
            ADMUX  = adchT; // select the negative input
            ADCSRA = 0x04;
            ADCSRB = 0x40;

            // start time1 with pre=1/8 (2MHz)
            // input capture noise canceler ON
            TCCR1B = (osctrig & 0x10) ? 0xc2 : 0x82;  // edge selection
            ACSR   = 0x14; // capture-on, aco to caputure timer1
            TIFR1  = 0x27; // clear flags again

            ui1 = (tokadif * toka) + (osctdly << 1);

            // falling edge detection(rising edge for ICES1)
            // doesn't stabilize without a 20usec wait below.
            while(TCNT1 < 40)
                ;
            TIFR1 = 0x27;
            // wait until a trigger event happens
            while(true) {
                if (TIFR1 & 0x20) {
                    // trigger event has happened.
                    ui1 += ICR1;
                    at = 0; // a trigger event has happened.
                    break;
                }
                if (TCNT1 > waituntil) {
                    ui1 += TCNT1;
                    at = 1; // trigger failed.
                    break;
                }
                uartjob();
            }

            // at:0 -> trigger event has happened, 1 -> not happened
            ACSR   = 0x94; // disable analog comparator
            ADCSRB = 0x00;
            ADCSRA = 0x84; // adc enable

            TCCR1B = 0x1a; // timer1 CTC-ICR1 mode pre1/8
            TCCR1A = 0x00; //             CTC mode;
            ICR1   = ui1;
            TIFR1  = 0x27; // clear flags

            ADMUX  = 0x60; // adc target is A0 pin to get trigger value;
            ADCSRB = 0x07; // timer1 capture event;
            ADCSRA = 0xf4; // adc auto trigger, force 1st conversion

            // wait until the 1st conversion finishes. 
            while((ADCSRA & 0x10) == 0x00)
                uartjob();
            vh = ADCH;  // trigger level
            osctvolt = vh;

            ADMUX = adch + ch;
            ADCSRA = 0xb4;   // clear flag, 1MHz, adate on

            if (toka == 0 && ch == 0) {   // needed only for the 1st loop
                // MARC-A  continued
                txput1(at);
                txput1(vh);
                txputcrc(false);
                txgo(); // start to trasmit a packet
                if (at)
                    goto  ex;  // send header only when trigger failed
            }

            for(i = 0; i < realnum; i++) {
                while(true) {
                    if (TIFR1 & 0x20) {
                        ICR1 = sinterval;
                        TIFR1 = 0x27; // clear timer1 flags;
                    }
                    if ((ADCSRA & 0x10) != 0x00)
                        break;
                    uartjob();
                }
                vh = ADCH;
                ADCSRA = 0xb4;   // clear flag, 1MHz, adate on
                txput1(vh);

                if (++crcnt >= 200) {
                    crcnt = 0;
                    // cause crc error on purpose if trigger failed(at > 0).
                    txputcrc((at > 0) ? true : false);
                }
            }
        }
    }
    //if (crcnt > 0)
    //    sysdown(800);
    if (crcnt == 40) {
        txputcrc((at > 0) ? true : false);
    }
    else
        sysdown(800);


ex:
    txfinish(true, true);
}


void
moderoll()
{
    byte    i;

    if (rmon == 0) {                         // start rollmode
        // reset and initialize timer1
        TCCR1B = 0x00;   // stop
        TCCR1A = 0x00;
        TIMSK1 = 0x00;   // no irq
        TCNT1  = 0x0000;
        ICR1   = 200;    // 100 usec
        TIFR1  = 0x27;   // clear flags;

        ACSR   = 0x94; // disable analog comparator
        ADCSRB = 0x00;
        ADCSRA = 0x84; // adc enable
        ADMUX  = 0x60; // adc target is A0 pin to get trigger value;
        ADCSRB = 0x07; // timer1 capture event;
        ADCSRA = 0xf4; // adc auto trigger, force start 1st conversion

        TCCR1B = 0x1a; // timer1 CTC-ICR1 mode pre1/8
        TCCR1A = 0x00; //             CTC mode;

        // wait until the 1st conversion finishes. 
        while((ADCSRA & 0x10) == 0x00)
            uartjob();
        osctvolt = ADCH;  // trigger level

        ADMUX  = 0x61;
        ADCSRA = 0xb4;   // clear flag, 1MHz, adate on

        rminit(true);
    }

    header(3, 0);

    txput1(0);
    txput1(osctvolt);
    txputcrc(false);

    txgo(); // start to trasmit a packet

    for(i = 0; i < 200; i++) {
        while(rmw == rmr)
            uartjob();
        txput1(rmbuf[rmr++]);
    }
    txputcrc(false);

    txfinish(true, true);
}


void
oscinfo(boolean restarted, byte id)
{
    if (restarted) {
        int    i;

        // send 300 bytes dummy data, since the host may be
        // receiving a 200 byte long packet. otherwise,
        // the first 201 bytes at most may be thrown away.
        txinit();
        for(i = 0; i < 300; i++)
            txput0(0);
        txgo();
        txfinish(false, false);
    }

    header(0, id);

    txput1(8);         // size of the additional info (bytes)
    txput1(osctvolt);
    txputcrc(false);
    uartjob();

    // additional info starts here
    txput1(oscversion >> 8);
    txput1(oscversion);
    txput1(oscconfig >> 8);
    txput1(oscconfig);
    uartjob();
    txput1(oscvbg >> 8);
    txput1(oscvbg >> 0);
    txput1((restarted) ? 1 : 0);
    txput1(fgen);
    uartjob();
    // additional info ended

    txputcrc(false);
    txgo();
    txfinish(true, true);
}


void
cupgain_set(byte val)
{
    if (cfg_cupgain != 2)
        return;

    val = ~val & 0x3f;
    osccupgain = val;
    PORTB = (PORTB & 0xe0) | (val >> 1);
    PORTD = (PORTD & 0x7f) | ((val & 1) ? 0x80 : 0x00);
}


void
cupgain_get()
{
    byte    val;

    if (cfg_cupgain == 2)
        return;

    val = (PINB & 0x1f) << 1;
    if (PIND & 0x80)
        val |= 1;
    osccupgain = ~val & 0x3f;
}


void
cupgain_init(byte ini)
{
    if (cfg_cupgain == 2) {
        DDRB |= 0x1f;
        DDRD |= 0x80;
        cupgain_set(ini);
    }
    else {
        DDRB  &= 0xe0;
        DDRD  &= 0x7f;
        if (cfg_cupgain == 1) {
            PORTB |= 0x1f;
            PORTD |= 0x80;
        }
        else {
            PORTB &= 0xe0;
            PORTD &= 0x7f;
        }
        cupgain_get();
    }
}


word
oscadc10(byte mux)
{
    word    val, tmp;
    int     i, j;

    // reset and initialize timer1
    TCCR1B = 0x00;
    TCCR1A = 0x00;
    TIMSK1 = 0x00; // no irq
    ICR1   = 0x0000;
    TCNT1  = 0x0000;
    TIFR1  = 0x27; // clear flags;

    ACSR   = 0x90; // disable analog comparator
    ADCSRB = 0x00;
    ADCSRA = 0x97; // adc enable  pre=1/128.  i.e. adc clock = 12.5khz
    ADMUX  = 0x40 | (mux & 0xf);

    // when bandgap reference is selected as a source,
    // a certain interval is needed for the stabilization
    // of the bandgap voltage.
    tmp = 0;
    i = 0;
    for(j = 5000; j > 0; j--) {
        ADCSRA = 0xd7; // adc start
        while((ADCSRA & 0x10) == 0x00)
            uartjob();
        val = (word)ADCL;
        val |= ((word)ADCH << 8);
        if (tmp != val) {
            tmp = val;
            i = 0;
            continue;
        }
        // if the same value continued 5 times, regard it as
        // the stabilized result
        if (++i >= 5)
            return  val;
    }
    return  0xffff;  // didn't stabilize
}


void
oscinit()
{
    word    val0, val1;

    if ((oscvbg = oscadc10(0xe)) == 0xffff) // bandgap voltage
        oscvbg = 0;

    cupgain_init(osccupgain);
    if (cfg_cupgain == 2)
        oscconfig |= 4;

    initt0();  // start trigger level generator

    // test if the trigger level generator circuit is equipped.
    sett0(192);
    wait0(4500, false);  // wai 72ms
    val1 = oscadc10(0);
    sett0( 64);
    wait0(4500, false);  // wai 72ms
    val0 = oscadc10(0);
    if ((val1 | val0) != 0xffff) {
        if (abs(val0 - 0x100) < 0x10 && abs(val1 - 0x300) < 0x10)
            oscconfig |= 1;  // yes, it worked properly.
    }
    osctduty = 0x80;
    sett0(osctduty);

    sett2(oscofreq, oscoduty);

}


// trigger level pwm voltage at D5 pin using timer0.
// Remember that time0 provides delay().
// So, millis(), micros(), delay(), and delayMicroseconds()
// are no longer available.

void
initt0()
{
    TCCR0B = 0x00; // stop timer0
    TCCR0A = 0x23; // FastPWM mode3 with OC0B output (D5)
    TIMSK0 = 0x00; // Disable all timer0 irqs
    TIFR0  = 0x07; // clear flags
    TCNT0  = 0x00;
    OCR0B  = 0x80; // duty 50%
    TCCR0B = 0x01; // start timer0  pre = 1/1  i.e 16MHz
}


void
wait0(word num, boolean uart)      // wait (num * 16) usecs
{
    while(num-- != 0) {
        TIFR0  = 0x07; // clear flags
        while((TIFR0 & 1) == 0) {
            if (uart)
                uartjob();
        }
    }
}


void
sett0(byte duty)
{
    if (duty == 0) {   // special care is needed to produce just low level
        TCCR0A = 0x33;
        OCR0B  = 0xff;
    }
    else {
        TCCR0A = 0x23;
        OCR0B = duty;   // 0..255
    }
}


// calibration oscillator using timer2
// outout pin is D3
// when hz == 0, change the D3 mode to Hi-Z.
void
sett2(long hz, byte duty) {
    long   top;
    int    ocr;
    byte   cmd2a, pre;

    if (hz < 1) {
        oscofreq = 0;
        oscoduty = 0;
        TCCR2B = 0x00;  // stop timer2
        TCCR2A = 0x00;
        DDRD  &= 0xf7;  // D3 is input
        PORTD &= 0xf7;  // D3 is Hi-Z
        return;
    }
    else {
        PORTD &= 0xf7;  // D3 is low
        DDRD  |= 0x08;  // D3 is output
    }

    hz = constrain(hz, 31, 2000000);  // 31Hz .. 2MHz
    oscofreq = hz;
    oscoduty = duty;

    cmd2a = 0x23;

    if (hz >= 62500L) {
        pre = 1;  // pre = 1/1 i.e. F_CPU = 16MHz    1..256
        top = 32000000L;
    }
    else if (hz >= 7813L) {
        pre = 2;  // pre = 1/8 i.e. F_CPU/8 = 2MHz   32..256
        top = 4000000L;
    }
    else if (hz >= 1954L) {
        pre = 3;  // pre = 1/32 i.e. F_CPU/32 = 500KHz  64..256
        top = 1000000L;
    }
    else if (hz >= 977L) {
        pre = 4;  // pre = 1/64 i.e. F_CPU/64 = 250KHz  128..256
        top = 500000L;
    }
    else if (hz >= 489L) {
        pre = 5;  // pre = 1/128i.e. F_CPU/128= 125KHz  128..256
        top = 250000L;
    }
    else if (hz >=  245L) {
        pre = 6;  // pre = 1/256 i.e. F_CPU/256= 62.5KHz  128..256
        top = 125000L;
    }
    else if (hz >= 61L) {
        pre = 7;  // pre = 1/1024 i.e. F_CPU/1024 = 15625KHz  64..256
        top = 31250L;
    }
    else {
        if (hz < 31)
            hz = 31L;
        cmd2a = 0x21; // or 0x31
        pre = 7;  // pre = 1/1024 i.e. F_CPU/1024 = 15625KHz  130..252
        top = 15625L;
    }
    top = ((top + hz) / hz) >> 1;
    ocr = (int)((top * (long)duty + 50L) / 100L);
    top--;
    if (ocr > top || (cmd2a != 0x21 && ocr > 0))
        ocr--;

    // special care for 0%
    if (ocr == 0)
        cmd2a = 0x21;

    TCCR2B = 0x00; // disable ocr2x buffering
    TCCR2A = 0x00;
    TCNT2  = 0x00;
    OCR2A  = (byte)top;
    OCR2B  = (byte)ocr;
    TCCR2B = 0x08;
    TCCR2A = cmd2a;
    TCCR2B |= pre;  // start timer2
}


byte
nib2asc(byte nib)
{
    return  (nib >= 10) ? (nib - 10 + 'a') : (nib + '0');
}


void
bin2hex(byte dat)
{
    txput0(nib2asc((dat >> 4) & 0xf));
    txput0(nib2asc((dat >> 0) & 0xf));
}


boolean
gen_cmd(const byte *p, byte id)
{
    static const byte defpkt[13] = {'M', 8};  // set memory DIPSW
    byte       sum, i, c, n;
    int        j;
    boolean    fgenconnected;

    fgenconnected = false;

    if (p == 0) {
        p = &defpkt[0];
        fgen = 255;
    }

    txfinish(false, false);
    txinit();
    PORTD &= 0xfb;  // D2 = LOW
    UDR0 = '%'; // tell funcgen to exit the gen loop
    for(j = 0; j < 1200; j++) // about 2msec wait
        uartjob();
    txput0('%');
    txput0('%');
    txput0('%');
    txput0('S');
    sum = p[0];
    txput0(sum);
    for(i = 1; i < 13; i++) {
        sum += p[i];
        bin2hex(p[i]);
        uartjob();
    }
    bin2hex(sum);
    txput0('\r');
    txput0('\n');
    txgo();
    txfinish(false, false);

    header(4, id);
    rxinit();
    sum = c = 0;
    for(j = 0; j < 20000; j++) {
        uartjob();
        n = rxnum();
        if (sum == n)
            continue;
        sum = n;
        c = rxbuf[n - 1];
        if (c == '=' || sum >= 200)
            break;
    }
    if (c != '=')
        goto  ex;
    fgenconnected = true;
    if (fgen == 255)
        fgen = rxbuf[n - 2] - '0';  // dipsw value 0..3

    txput1(sum);
    txput1(osctvolt);
    txputcrc(false);

    for(i = 0; i < sum; i++)
        txput1(rxbuf[i]);
    txputcrc(false);

    PORTD |= 0x04;  // D2 = HIGH
    txgo();
    txfinish(false, true);

ex:
    PORTD |= 0x04;  // D2 = HIGH
    rxinit();
    txinit();
    return  fgenconnected;
}


void
cmdproc()
{
    if (rxbuf[1] == 'A') {
        long  hz;
        int   treq;

        oscspeed = rxbuf[3];
        oscinput = rxbuf[4];
        osctrig  = rxbuf[5];
        osctdly  = rxbuf[8];
        uartjob();
        osctdly  = (osctdly << 8) | rxbuf[9];
        osctdly  = constrain(osctdly, 100, 30000);
        uartjob();

        treq = (int)rxbuf[6];
        if (treq > 0) {
            if (treq == 255) {
                treq = 128;  // reset
            }
            else if (treq == 254) {
                treq = (int)rxbuf[7];
            }
            else {
                treq = osctduty + (treq - 60);
            }
            treq = constrain(treq, 0, 255);
            uartjob();
            if (treq != osctduty) {
                osctduty = (byte)treq;
                sett0(osctduty);
                uartjob();
            }
        }
        hz = rxbuf[10];
        hz = (hz << 8) | rxbuf[11];
        hz = (hz << 8) | rxbuf[12];
        uartjob();
        sett2(hz, rxbuf[13]);
        uartjob();
        cupgain_set(rxbuf[14]);
        uartjob();

        // when osc settings are changed, send an info packet.
        oscinfo(false, rxbuf[2]);

    }
    else if (rxbuf[1] == 'G') {
        gen_cmd(&rxbuf[3], rxbuf[2]);
    }
    else if (rxbuf[1] == 'Q') {
        // freeze for about 5 seconds
        byte   sec;

        header(5, rxbuf[2]);
        txput1(0);         // size of the additional info (bytes)
        txput1(osctvolt);
        txputcrc(false);
        txgo();
        txfinish(true, true);

        rminit(false);
        rxinit();
        txinit();
        for(sec = 0; sec < 20; sec++) {
            PORTB |= 0x20;  // D13 led-on
            wait0(3125, false);
            PORTB &= 0xdf;  // D13 led-off
            wait0(3125, false);
        }
    }
    uartjob();
}


#define CMDLEN 16

void
cmdrecv() {
    static byte  reqid = 0;
    byte  crc, i, num;
    int    j, retry;

    retry = 1;
    for(j = 0; j < retry; j++) {
        if (j > 0)
            wait0(1, true);
        num = rxnum();
        if (num == 0)
            continue;
        if (rxbuf[0] != '$') {
            rxinit();
            continue;
        }
        if (num < (CMDLEN + 1))
            continue;
        crc = 0;
        for(i = 0; i < CMDLEN; i++) {
            crc = crctbl[crc ^ rxbuf[i]];
            uartjob();
        }
        if (rxbuf[CMDLEN] != crc) {
            retry = 20000;  // max 320msecs
            rxinit();
            continue;
        }
        if (rxbuf[2] != reqid) {
            reqid = rxbuf[2];
            cmdproc();
        }
        rxinit();
        break;
    }
    uartjob();
}


void
loop()
{
    cupgain_get();

    if (oscspeed == 8)
        moderoll();
    else if (oscspeed >= 4)
        modeequiv();
    else
        modereal();
    cmdrecv();
}


void
setup()
{
    cli();

    DDRB  |= 0x20;   // output  D13
    PORTB &= 0xdf;   // D13 = LOW
    DDRD  |= 0x2c;   // output  D2, D3, D5
    PORTD |= 0x04;   // D2 == HIGH  disconnect the funcgen

    UCSR0A = 0x02;   // uart async 230400bps 8bit non-parity 1 stopbit
    UBRR0H = 0x00;
    UBRR0L = 0x08;
    UCSR0C = 0x06;
    UCSR0B = 0x18;

    txinit();
    rxinit();
    rminit(false);

    oscinit();

    // when the function generator seems not to be attached,
    // check again up to 2 more times.
    if (gen_cmd(0, 0) || gen_cmd(0, 0) || gen_cmd(0, 0))
        oscconfig |= 2;  // fgen is connected
    oscinfo(true, 0);
}


void
sysdown(int dly)  // dly .. in msec
{
    int    i;
    byte   s;

    SPCR   = 0x00; // disable SPI
    TCCR0B = 0x00; // stop timer0
    TCCR0A = 0x02; // ctc mode
    TIMSK0 = 0x00; // Disable all timer0 irqs
    TIFR0  = 0x07; // clear flags
    TCNT0  = 0x00;
    OCR0A  = 250 - 1;
    TCCR0B = 0x03; // start timer0  pre = 1/64  i.e 250kHz

    s = 0;
    while(true) {
        if (++s & 1)
            PORTB |= 0x20;  // D13 == HIGH  (LED on)
        else
            PORTB &= 0xdf;  // D13 == LOW   (LED off)
        for(i = 0; i < dly; i++) {
            while((TIFR0 & 2) == 0)
                ;
            TIFR0  = 0x07; // clear flags
        }
    }
}
