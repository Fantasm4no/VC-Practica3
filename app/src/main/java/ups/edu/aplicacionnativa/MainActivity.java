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
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

import android.os.Environment;
import android.media.MediaScannerConnection;
import android.content.pm.PackageManager;

import androidx.appcompat.app.AlertDialog;

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

        if (getSupportActionBar() != null) {
            getSupportActionBar().hide();
        }

        if (checkSelfPermission(android.Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{android.Manifest.permission.READ_EXTERNAL_STORAGE}, 1);
        }

        super.onCreate(savedInstanceState);
        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        loadDescriptorsFromAssets();
        sendDescriptorsToNative();

        binding.buttonProcess.setOnClickListener(v -> {
            Bitmap bitmap = binding.drawingView.getBitmapResized(256, 256);
            if (bitmap != null) {
                String real = binding.spinnerExpectedLabel.getSelectedItem().toString();
                String label = classifyShapeNative(bitmap);

                resultados.add(new Resultado(real, label));
                binding.textResult.setText(label);

                String timestamp = new SimpleDateFormat("yyyyMMdd_HHmmss", Locale.getDefault()).format(new Date());

                String nombreArchivo = "dibujo_" + timestamp + "_REAL-" + real + "_PRED-" + label;
                guardarDibujoComoImagen(bitmap, nombreArchivo);

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

        binding.buttonShowReport.setOnClickListener(v -> mostrarReporte());
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

    public static class Resultado {
        public final String real;
        public final String predicho;

        public Resultado(String real, String predicho) {
            this.real = real;
            this.predicho = predicho;
        }
    }

    private final List<Resultado> resultados = new ArrayList<>();

    private void mostrarReporte() {
        String[] clases = {"Circle", "Square", "Triangle"};
        int[][] matriz = new int[3][3];
        int aciertos = 0;

        for (Resultado r : resultados) {
            int i = indexOf(r.real, clases);
            int j = indexOf(r.predicho, clases);
            if (i >= 0 && j >= 0) {
                matriz[i][j]++;
                if (i == j) aciertos++;
            }
        }

        int total = resultados.size();
        double precision = total > 0 ? (double) aciertos / total : 0.0;

        // Formatear matriz como tabla de texto
        StringBuilder matrixBuilder = new StringBuilder();
        matrixBuilder.append(String.format("%-10s%-10s%-10s%-10s\n", "", "Circle", "Square", "Triangle"));
        for (int i = 0; i < 3; i++) {
            matrixBuilder.append(String.format("%-10s", clases[i]));
            for (int j = 0; j < 3; j++) {
                matrixBuilder.append(String.format("%-10d", matriz[i][j]));
            }
            matrixBuilder.append("\n");
        }

        String resumen = "🧮 Total pruebas: " + total +
                "\n✅ Aciertos: " + aciertos +
                "\n📈 Precisión: " + String.format("%.2f%%", precision * 100);

        // Inflar layout personalizado
        LayoutInflater inflater = LayoutInflater.from(this);
        View dialogView = inflater.inflate(R.layout.dialog_reporte, null);

        TextView textMatrix = dialogView.findViewById(R.id.textMatrix);
        TextView textResumen = dialogView.findViewById(R.id.textResumen);

        textMatrix.setText(matrixBuilder.toString());
        textResumen.setText(resumen);

        new AlertDialog.Builder(this)
                .setView(dialogView)
                .setPositiveButton("OK", null)
                .setNegativeButton("Limpiar Resultados", (dialog, which) -> {
                    resultados.clear();
                    Toast.makeText(this, "Resultados limpiados", Toast.LENGTH_SHORT).show();
                })
                .show();
    }

    private int indexOf(String valor, String[] array) {
        for (int i = 0; i < array.length; i++) {
            if (array[i].equalsIgnoreCase(valor)) return i;
        }
        return -1;
    }

    private void guardarDibujoComoImagen(Bitmap bitmap, String nombreArchivo) {
        File directorio = new File(Environment.getExternalStoragePublicDirectory(
                Environment.DIRECTORY_PICTURES), "DibujosClasificador");

        if (!directorio.exists()) {
            directorio.mkdirs(); // crea la carpeta si no existe
        }

        File archivo = new File(directorio, nombreArchivo + ".png");

        try (FileOutputStream out = new FileOutputStream(archivo)) {
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);

            // Notificar al sistema para que lo indexe y aparezca en la galería
            MediaScannerConnection.scanFile(this,
                    new String[]{archivo.getAbsolutePath()},
                    new String[]{"image/png"}, null);

            Toast.makeText(this, "Guardado en Galería:\n" + archivo.getAbsolutePath(), Toast.LENGTH_SHORT).show();
        } catch (IOException e) {
            e.printStackTrace();
            Toast.makeText(this, "Error al guardar imagen", Toast.LENGTH_SHORT).show();
        }
    }

}
