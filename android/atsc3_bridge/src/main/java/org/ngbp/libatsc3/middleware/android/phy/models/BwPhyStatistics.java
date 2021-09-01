package org.ngbp.libatsc3.middleware.android.phy.models;

public class BwPhyStatistics {
    public static long AppStartTimeMs = System.currentTimeMillis();

    public long total_pkts;
    public long total_bytes;
    public int total_lmts;

    public BwPhyStatistics(long total_pkts, long total_bytes, int total_lmts) {
        this.total_pkts = total_pkts;
        this.total_bytes = total_bytes;
        this.total_lmts = total_lmts;
    }

    public static long computeLast1sBwKbitTimestamp = 0;
    public static long computeLast1sBwKbitStartValue = 0;

    public static float ComputeLast1sBwBitsSec(long total_bytes) {
        long currentTimeMs = System.currentTimeMillis();
        float avgValue = 0;
        if(computeLast1sBwKbitTimestamp == 0) {
            computeLast1sBwKbitTimestamp = currentTimeMs;
            computeLast1sBwKbitStartValue = total_bytes;
        } else {
            avgValue = (float)(8*(total_bytes - computeLast1sBwKbitStartValue)) / ((currentTimeMs - computeLast1sBwKbitTimestamp)/1000.0f);
            if(currentTimeMs - computeLast1sBwKbitTimestamp >= 1000) {
                computeLast1sBwKbitTimestamp = currentTimeMs;
                computeLast1sBwKbitStartValue = total_bytes;
            }
        }
        return avgValue;
    }

    public static long computeLast1sBwPPSTimestamp = 0;
    public static long computeLast1sBwPPSStartValue = 0;

    public static float ComputeLast1sBwPPS(long total_pkts) {
        long currentTimeMs = System.currentTimeMillis();
        float avgValue = 0;
        if(computeLast1sBwPPSTimestamp == 0) {
            computeLast1sBwPPSTimestamp = currentTimeMs;
            computeLast1sBwPPSStartValue = total_pkts;
        } else {
            avgValue = (float)(total_pkts - computeLast1sBwPPSStartValue) / ((currentTimeMs - computeLast1sBwPPSTimestamp)/1000.0f);
            if(currentTimeMs - computeLast1sBwPPSTimestamp >= 1000) {
                computeLast1sBwPPSTimestamp = currentTimeMs;
                computeLast1sBwPPSStartValue = total_pkts;
            }
        }
        return avgValue;
    }

    Boolean hasCalledComputeBwMetrics = false;

    double currentRuntimeDurationS = 0;
    float last_1s_bw_bitsSec = 0;
    float last_1s_bw_pps = 0;

    private void computeBwMetricsOnce() {
        if(!hasCalledComputeBwMetrics) {
            currentRuntimeDurationS = (float) (System.currentTimeMillis() - AppStartTimeMs) / 1000.0;
            last_1s_bw_bitsSec = ComputeLast1sBwBitsSec(this.total_bytes);
            last_1s_bw_pps = ComputeLast1sBwPPS(this.total_pkts);
        }
        hasCalledComputeBwMetrics = true;
    }

    @Override
    public String toString() {
        computeBwMetricsOnce();

        return String.format("Last 1s: %.2f Mbit/sec, %.0f PPS\n"+
                             "Totals : Bytes: %.2f MB, Packets: %d, LMT: %d\n" + "" +
                             "Runtime: %.2fs, Sess. Avg: %.2f Mbit/sec",
                (float) last_1s_bw_bitsSec / (1024.0 * 1024.0),
                last_1s_bw_pps,
                (float) (this.total_bytes / (1024.0 * 1024.0)),
                this.total_pkts,
                this.total_lmts,
                (float) ((this.total_bytes * 8.0) / (1024.0 * 1024.0)) / currentRuntimeDurationS,
                currentRuntimeDurationS
        );
    }
}
