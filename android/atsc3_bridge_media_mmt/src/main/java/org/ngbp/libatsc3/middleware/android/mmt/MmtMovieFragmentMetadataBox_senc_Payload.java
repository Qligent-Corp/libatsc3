package org.ngbp.libatsc3.middleware.android.mmt;

import android.util.Log;

import java.nio.ByteBuffer;
import java.util.ArrayList;

public class MmtMovieFragmentMetadataBox_senc_Payload {
    /*
    From ISO/IEC 23001-7:2016(E): Section 7.2.1 Sample Encryption Box 'senc'

        aligned(8) class SampleEncryptionBox
           extends FullBox(‘senc’, version=0, flags)
        {
           unsigned int(32) sample_count;
           {
              unsigned int(Per_Sample_IV_Size*8) InitializationVector;
              if (flags & 0x000002)
              {
                 unsigned int(16) subsample_count;
                 {
                    unsigned int(16) BytesOfClearData;
                    unsigned int(32) BytesOfProtectedData;
                 } [ subsample_count ]
                 }
           }[ sample_count ]
        }
     */

    public static class senc_box {
        //box
        public Integer     size;
        public byte[]      type = new byte[4];

        //full box
        public byte        version;                  //version=0
        public byte[]      flags = new byte[3];      //technically 24 bits

        //senc
        public Integer     sample_count = 0;

        public static class sample {
            public byte[]     iv = new byte[16];    //assuming 16 byte IV?
            public int        iv_tenc_size = 16;    //assuming 16 byte IV...
            public Short      subsample_count = 0; //set to 0 so we can at least compare without a null check16 bits

            public static class subsample {
                public Short                   bytes_of_clear_data;         //16 bits
                public Integer                 bytes_of_protected_data;    //32 bits
            }

            public  ArrayList<subsample> subsampleArrayList = new ArrayList<subsample>();

            //roll-up
            public ArrayList<Short>     subsample_bytes_of_clear_data_arrayList = new ArrayList<>();
            public ArrayList<Integer>   subsample_bytes_of_protected_data_arrayList = new ArrayList<>();

        }
        public ArrayList<sample> sampleArrayList = new ArrayList<>();
    }

    public int          packet_id;
    public long         mpu_sequence_number; //technically, mpu_sequence_number is uint32_t, so we need int64_t to handle it properly in java

    public ByteBuffer   byteBuffer;
    public int          byteBuffer_length;

    public senc_box     senc = new senc_box();



    public MmtMovieFragmentMetadataBox_senc_Payload(int packet_id, long mpu_sequence_number, ByteBuffer nativeByteBuffer, int length) {
        this.packet_id = packet_id;
        this.mpu_sequence_number = mpu_sequence_number;

        byteBuffer = ByteBuffer.allocate(length);
        nativeByteBuffer.rewind();
        byteBuffer.put(nativeByteBuffer);
        byteBuffer.rewind();

        byteBuffer_length = length;

        //parse our our senc box

        senc.size = byteBuffer.getInt();
        byteBuffer.get(senc.type);

        senc.version = byteBuffer.get();
        byteBuffer.get(senc.flags);

        senc.sample_count = byteBuffer.getInt();

        /*

        jjustman-2022-01-28 - todo: parse

    seig

        aligned(8) class CencSampleEncryptionInformationGroupEntry

             aligned(8) class TrackEncryptionBox extends FullBox(‘tenc’, version, flags=0)

        Tracks of all types SHALL use the CencSampleEncryptionInformationGroupEntry sample group description structure, which has the following syntax.
        {
        extends SampleGroupEntry( ‘seig’ )
        unsigned int(8)
        unsigned int(4)
        unsigned int(4)
        unsigned int(8)
        unsigned int(8) Per_sample_IV_size
        unsigned int(8)[16]
        if (isProtected ==1 && Per_Sample_IV_Size == 0) {
        unsigned int(8) constant_IV_size;
        }


OR

    tenc

        to get proper Per_Sample_IV_Size

         */
int last_i = 0;

        try {

            for (int i = 0; i < senc.sample_count; i++) {
            last_i = i;
                senc_box.sample sample = new senc_box.sample();
                byteBuffer.get(sample.iv, 0, sample.iv_tenc_size);

                //short-hand ref
                if ((senc.flags[2] & 0x000002) == 0x000002) {
                    sample.subsample_count = byteBuffer.getShort();

                    for (int j = 0; j < sample.subsample_count; j++) {
                        senc_box.sample.subsample subsample = new senc_box.sample.subsample();
                        subsample.bytes_of_clear_data = byteBuffer.getShort();
                        subsample.bytes_of_protected_data = byteBuffer.getInt();
                        sample.subsampleArrayList.add(subsample);

                        sample.subsample_bytes_of_clear_data_arrayList.add(j, subsample.bytes_of_clear_data);
                        sample.subsample_bytes_of_protected_data_arrayList.add(j, subsample.bytes_of_protected_data);

                    }
                }

                senc.sampleArrayList.add(sample);
            }
        } catch (Exception ex) {
            Log.w("senc_exception", ex);

        }
    }

    public void releaseByteBuffer() {
        if(byteBuffer != null) {
            byteBuffer.clear();
        }

        byteBuffer = null;
    }
}
