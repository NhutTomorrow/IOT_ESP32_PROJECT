#include "tinyml.h"

// Globals, for the convenience of one-shot setup.
namespace
{
    tflite::ErrorReporter *error_reporter = nullptr;
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;
    constexpr int kTensorArenaSize = 8 * 1024; // Adjust size based on your model
    uint8_t tensor_arena[kTensorArenaSize];
} // namespace

// A small hardcoded test dataset for evaluation
// Format: {Temperature, Humidity, Expected Label (0=Normal, 1=Warning, 2=Critical)}
const float test_dataset[][3] = {
    {29.5, 64.0, 0}, {26.7, 45.3, 0}, {28.7, 63.0, 0}, {28.9, 57.4, 0},
    {28.8, 60.0, 0}, {29.7, 54.6, 0}, {28.6, 49.0, 0}, {28.9, 47.3, 0},
    {27.3, 46.5, 0}, {29.5, 48.8, 0}, {28.6, 58.5, 0}, {29.7, 61.7, 0},
    {28.5, 51.2, 0}, {27.5, 61.4, 0}, {27.9, 48.8, 0}, {26.8, 53.5, 0},
    {27.7, 60.5, 0},
    {30.7, 62.2, 1}, {34.9, 55.8, 1}, {27.7, 38.5, 1}, {28.2, 42.1, 1},
    {33.8, 55.5, 1}, {26.6, 68.1, 1}, {21.4, 49.4, 1}, {25.7, 49.7, 1},
    {25.5, 59.0, 1}, {23.2, 54.6, 1}, {33.0, 50.0, 1}, {31.0, 50.4, 1},
    {28.0, 35.8, 1}, {20.2, 54.9, 1}, {30.9, 60.6, 1}, {20.7, 46.9, 1},
    {25.3, 52.7, 1},
    {10.7, 67.9, 2}, {21.5, 90.6, 2}, {46.5, 29.3, 2}, {39.6, 30.1, 2},
    {37.8, 41.2, 2}, {40.1, 21.0, 2}, {27.6, 21.2, 2}, {32.2, 90.2, 2},
    {16.5, 35.5, 2}, {16.3, 24.7, 2}, {38.3, 46.3, 2}, {47.8, 32.2, 2},
    {36.4, 60.4, 2}, {10.4, 88.8, 2}, {37.2, 34.7, 2}, {19.4, 72.5, 2}
};

void evaluateModelOnHardware() {
    Serial.println("--- Starting Hardware Evaluation ---");
    
    int total_samples = sizeof(test_dataset) / sizeof(test_dataset[0]);
    int correct_predictions = 0;
    unsigned long total_inference_time_us = 0;

    for (int i = 0; i < total_samples; i++) {
        // 1. Prepare input
        input->data.f[0] = test_dataset[i][0]; // Temperature
        input->data.f[1] = test_dataset[i][1]; // Humidity
        int expected_label = (int)test_dataset[i][2];

        // 2. Measure inference time
        unsigned long start_time = micros();
        TfLiteStatus invoke_status = interpreter->Invoke();
        unsigned long end_time = micros();

        if (invoke_status != kTfLiteOk) {
            Serial.println("Inference failed!");
            continue;
        }

        // 3. Accumulate latency
        unsigned long inference_time = end_time - start_time;
        total_inference_time_us += inference_time;

        // 4. Determine prediction
        float prob_normal = output->data.f[0];
        float prob_warning = output->data.f[1];
        float prob_critical = output->data.f[2];

        int predicted_label = 0;
        float max_prob = prob_normal;
        if (prob_warning > max_prob) { predicted_label = 1; max_prob = prob_warning; }
        if (prob_critical > max_prob) { predicted_label = 2; max_prob = prob_critical; }

        // 5. Check if correct
        if (predicted_label == expected_label) {
            correct_predictions++;
        }
    }

    // Calculate metrics
    float accuracy = ((float)correct_predictions / total_samples) * 100.0;
    float avg_latency_ms = (float)total_inference_time_us / (total_samples * 1000.0);

    Serial.printf("Hardware Accuracy: %.2f%% (%d/%d correct)\n", accuracy, correct_predictions, total_samples);
    Serial.printf("Average Inference Latency: %.3f ms\n", avg_latency_ms);
    Serial.printf("Tensor Arena Size: %d Bytes\n", kTensorArenaSize);
    Serial.println("--- Evaluation Complete ---");
}

void setupTinyML()
{
    Serial.println("TensorFlow Lite Init....");
    static tflite::MicroErrorReporter micro_error_reporter;
    error_reporter = &micro_error_reporter;

    model = tflite::GetModel(dht_anomaly_model_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        error_reporter->Report("Model provided is schema version %d, not equal to supported version %d.",
                               model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }

    static tflite::AllOpsResolver resolver;
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
    interpreter = &static_interpreter;

    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk)
    {
        error_reporter->Report("AllocateTensors() failed");
        return;
    }

    input = interpreter->input(0);
    output = interpreter->output(0);

    Serial.println("TensorFlow Lite Micro initialized on ESP32.");
    evaluateModelOnHardware();
}

void tiny_ml_task(void *pvParameters)
{
    system_se_t *sys = (system_se_t *)pvParameters;
    sensor_data_t current_data;
    ml_result_t ml_res;

    setupTinyML();

    while (1)
    {
        // Wait for new sensor data
        if (xQueuePeek(sys->queue_raw_data, &current_data, portMAX_DELAY) == pdTRUE)
        {
            // Prepare input data
            input->data.f[0] = current_data.temperature;
            input->data.f[1] = current_data.humidity;

            // Run inference
            TfLiteStatus invoke_status = interpreter->Invoke();
            if (invoke_status != kTfLiteOk)
            {
                error_reporter->Report("Invoke failed");
            }
            else
            {
                // Get output probabilities
                float prob_normal = output->data.f[0];
                float prob_warning = output->data.f[1];
                float prob_critical = output->data.f[2];

                // Determine label with max probability
                int label = 0;
                float max_prob = prob_normal;

                if (prob_warning > max_prob)
                {
                    label = 1;
                    max_prob = prob_warning;
                }
                if (prob_critical > max_prob)
                {
                    label = 2;
                    max_prob = prob_critical;
                }

                ml_res.label = label;
                ml_res.confidence = max_prob;

                Serial.printf("[TinyML] Infer: temp=%.1f humi=%.1f -> Label: %d (%.2f)\n", current_data.temperature, current_data.humidity, label, max_prob);

                // Publish to dedicated ML queue
                xQueueOverwrite(sys->queue_ml_result, &ml_res);
                
                // Signal that ML data is ready
                xSemaphoreGive(sys->se_ml_ready);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}