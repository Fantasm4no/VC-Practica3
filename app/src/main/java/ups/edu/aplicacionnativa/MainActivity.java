package ups.edu.aplicacionnativa;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.graphics.Bitmap;
import android.widget.Toast;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

import ups.edu.aplicacionnativa.databinding.ActivityMainBinding;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("aplicacionnativa");
    }

    private ActivityMainBinding binding;

    // Descriptor con momentos Hu y firma
    public static class Descriptor {
        public String label;
        public double[] huMoments;   // 7 valores
        public double[] signature;   // firma (longitud variable)

        public Descriptor(String label, double[] huMoments, double[] signature) {
            this.label = label;
            this.huMoments = huMoments;
            this.signature = signature;
        }
    }

    private List<Descriptor> descriptors = new ArrayList<>();

    // JNI methods
    public native void setDescriptorsNative(String[] labels, double[][] huMomentsArrays, double[][] signatureArrays);
    public native String classifyShapeNative(Bitmap bitmap);
    public native boolean isShapeFilled();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        loadDescriptorsFromAssets();
        sendDescriptorsToNative();

        binding.buttonProcess.setOnClickListener(v -> {
            Bitmap bitmap = binding.drawingView.getBitmapResized(256, 256);
            if (bitmap != null) {
                String label = classifyShapeNative(bitmap);
                binding.textResult.setText(label);

                if (!isShapeFilled()) {
                    Toast.makeText(this, "Advertencia: Figura no está cerrada correctamente.", Toast.LENGTH_LONG).show();
                }
            } else {
                Toast.makeText(this, "No hay dibujo", Toast.LENGTH_SHORT).show();
            }
        });

        binding.buttonClear.setOnClickListener(v -> {
            binding.drawingView.clear();
            binding.textResult.setText("Etiqueta detectada");
        });
    }

    private void loadDescriptorsFromAssets() {
        try {
            BufferedReader reader = new BufferedReader(new InputStreamReader(getAssets().open("descriptores.csv")));

            // Saltar encabezado
            reader.readLine();

            String line;
            while ((line = reader.readLine()) != null) {
                String[] tokens = line.split(",");
                if (tokens.length < 8) continue; // label + 7 hu moments mínimo

                String label = tokens[0].trim();

                double[] huMoments = new double[7];
                for (int i = 0; i < 7; i++) {
                    huMoments[i] = Double.parseDouble(tokens[i + 1].replace(",", ".")); // en caso decimal con coma
                }

                int sigLen = tokens.length - 8;
                double[] signature = new double[sigLen];
                for (int i = 0; i < sigLen; i++) {
                    signature[i] = Double.parseDouble(tokens[i + 8].replace(",", "."));
                }

                descriptors.add(new Descriptor(label, huMoments, signature));
            }

            reader.close();
            Toast.makeText(this, "Descriptores cargados: " + descriptors.size(), Toast.LENGTH_SHORT).show();
        } catch (Exception e) {
            e.printStackTrace();
            Toast.makeText(this, "Error cargando descriptores: " + e.getMessage(), Toast.LENGTH_LONG).show();
        }
    }

    private void sendDescriptorsToNative() {
        int n = descriptors.size();
        String[] labels = new String[n];
        double[][] huMomentsArrays = new double[n][7];

        // Averiguar máxima longitud de firma para rellenar arrays 2D
        int maxSigLen = 0;
        for (Descriptor d : descriptors) {
            maxSigLen = Math.max(maxSigLen, d.signature.length);
        }

        double[][] signatureArrays = new double[n][maxSigLen];
        for (int i = 0; i < n; i++) {
            Descriptor d = descriptors.get(i);
            labels[i] = d.label;
            huMomentsArrays[i] = d.huMoments;
            // Copiar firma y rellenar con ceros si es necesario
            for (int j = 0; j < maxSigLen; j++) {
                signatureArrays[i][j] = (j < d.signature.length) ? d.signature[j] : 0.0;
            }
        }

        setDescriptorsNative(labels, huMomentsArrays, signatureArrays);
    }
}
