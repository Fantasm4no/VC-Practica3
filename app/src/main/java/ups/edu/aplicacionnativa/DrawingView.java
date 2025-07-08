package ups.edu.aplicacionnativa;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

public class DrawingView extends View {

    private Paint paint;
    private Path path;

    // Variables para suavizar el trazo con curvas Bézier
    private float lastX, lastY;
    private static final float TOUCH_TOLERANCE = 3; // Menor para trazo más continuo

    public DrawingView(Context context, AttributeSet attrs) {
        super(context, attrs);

        paint = new Paint();
        paint.setAntiAlias(true);
        paint.setColor(Color.WHITE); // Dibujo en blanco
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(12); // Puedes ajustar este grosor
        paint.setStrokeJoin(Paint.Join.ROUND);
        paint.setStrokeCap(Paint.Cap.ROUND);

        path = new Path();

        setBackgroundColor(Color.BLACK); // Fondo negro
    }

    @Override
    protected void onDraw(Canvas canvas) {
        // Dibujar el path
        canvas.drawPath(path, paint);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        float x = event.getX();
        float y = event.getY();

        // Limitar el toque dentro de los límites del view
        x = Math.max(0, Math.min(x, getWidth()));
        y = Math.max(0, Math.min(y, getHeight()));

        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
                touchStart(x, y);
                return true;
            case MotionEvent.ACTION_MOVE:
                touchMove(x, y);
                break;
            case MotionEvent.ACTION_UP:
                touchUp();
                break;
        }

        invalidate();
        return true;
    }

    private void touchStart(float x, float y) {
        path.moveTo(x, y);
        lastX = x;
        lastY = y;
    }

    private void touchMove(float x, float y) {
        float dx = Math.abs(x - lastX);
        float dy = Math.abs(y - lastY);

        if (dx >= TOUCH_TOLERANCE || dy >= TOUCH_TOLERANCE) {
            // Usar quadTo para suavizar la línea con curva Bézier
            path.quadTo(lastX, lastY, (x + lastX) / 2, (y + lastY) / 2);
            lastX = x;
            lastY = y;
        }
    }

    private void touchUp() {
        // Completar la última línea
        path.lineTo(lastX, lastY);
    }

    // Método para limpiar el dibujo
    public void clear() {
        path.reset();
        invalidate();
    }

    // Método para obtener el Bitmap del dibujo actual con tamaño original del View
    public Bitmap getBitmap() {
        Bitmap bitmap = Bitmap.createBitmap(getWidth(), getHeight(),
                Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        draw(canvas);
        return bitmap;
    }

    // NUEVO: Método para obtener Bitmap redimensionado a tamaño fijo (ejemplo 256x256)
    public Bitmap getBitmapResized(int width, int height) {
        Bitmap original = getBitmap();
        return Bitmap.createScaledBitmap(original, width, height, true);
    }

    // Métodos para cambiar color o grosor dinámicamente (opcional)
    public void setPaintColor(int color) {
        paint.setColor(color);
    }

    public void setStrokeWidth(float width) {
        paint.setStrokeWidth(width);
    }
}
