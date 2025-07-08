package ups.edu.aplicacionnativa;

import java.util.List;

public class DescriptorResult {
    public String predictedLabel;
    public List<Double> huMoments;
    public List<Double> signature;

    public DescriptorResult(String predictedLabel, List<Double> huMoments, List<Double> signature) {
        this.predictedLabel = predictedLabel;
        this.huMoments = huMoments;
        this.signature = signature;
    }
}
