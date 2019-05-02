#include <linux/module.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/device.h>
#include <asm/page.h>
#include <linux/thermal.h>
#include "hisi_peripheral_tm.h"
#include <linux/power/hisi/coul/hisi_coul_drv.h>

#define CREATE_TRACE_POINTS
#include <trace/events/shell_temp.h>

#define MUTIPLY_FACTOR          (100000)
#define DEFAULT_SHELL_TEMP      (25000)
#define ERROR_SHELL_TEMP        (125000)
#define SENSOR_PARA_COUNT       3
#define BATTERY_NAME            "Battery"
#define INVALID_TEMP_OUT_OF_RANGE       1
#define DEFAULT_TSENS_MAX_TEMP          100000
#define DEFAULT_TSENS_MIN_TEMP          0
#define DEFAULT_TSENS_STEP_RANGE        4000
#define DEFAULT_NTC_MAX_TEMP            80000
#define DEFAULT_NTC_MIN_TEMP            -20000
#define DEFAULT_NTC_STEP_RANGE          2000
#define DEFAULT_SHELL_TEMP_STEP_RANGE   400
#define DEFAULT_SHELL_TEMP_STEP         200
#define HAVE_INVALID_SENSOR_TEMP        1
#define CHECK_TSENS                     0x80
#define CHECK_NTC                       0x7f

enum {
    TYPE_TSENS = 0x80,
    TYPE_PERIPHERAL = 0x01,
    TYPE_BATTERY = 0x02,
    TYPE_TERMINAL = 0x04,
    TYPE_UNKNOWN = 0x0,
};

struct hisi_temp_tracing_t {
    int temp;
    int coef;
    int temp_invalid_flag;
};

struct hisi_shell_sensor_t {
    const char *sensor_name;
    u32 sensor_type;
    struct hisi_temp_tracing_t temp_tracing[];
};

struct hisi_shell_t {
    int sensor_count;
    int sample_count;
    int tsensor_temp_step;
    int tsensor_max_temp;
    int tsensor_min_temp;
    int ntc_temp_step;
    int ntc_max_temp;
    int ntc_min_temp;
    int shell_temp_step_range;
    int shell_temp_step;
    u32 interval;
    int bias;
    int temp;
    int old_temp;
    int index;
    int valid_flag;
#ifdef CONFIG_HISI_SHELL_TEMP_DEBUG
    int channel;
    int debug_temp;
#endif
    struct thermal_zone_device *tz_dev;
    struct delayed_work work;
    struct hisi_shell_sensor_t *sensors[];
};

static inline size_t sensor_size(int sample_count)
{
    return sizeof(struct hisi_shell_sensor_t) + sample_count * sizeof(struct hisi_temp_tracing_t);
}

static inline struct hisi_shell_sensor_t *get_sensor(struct hisi_shell_t *shell, int idx)
{
    if (idx < 0 || idx >= shell->sensor_count)
        return NULL;
    return shell->sensors[idx];
}

static ssize_t hisi_shell_show_temp(struct device *dev, struct device_attribute *devattr, char *buf)
{
    struct hisi_shell_t *hisi_shell;

    if (!dev || !devattr || !dev->driver_data)
        return 0;

    hisi_shell = dev->driver_data;
    return snprintf(buf, PAGE_SIZE - 1, "%d\n", hisi_shell->temp);
}
static DEVICE_ATTR(temp, S_IWUSR | S_IRUGO, hisi_shell_show_temp, NULL);

#ifdef CONFIG_HISI_SHELL_TEMP_DEBUG
static ssize_t hisi_shell_store_debug_temp(struct device *dev, struct device_attribute *devattr,
                                           const char *buf, size_t count)
{
    int channel, temp;
    struct platform_device *pdev;
    struct hisi_shell_t *hisi_shell;

    if (!dev || !devattr)
        return 0;

    if (sscanf(buf, "%d %d", &channel, &temp) != 2) {
        pr_err("%s Invalid input para\n", __func__);
        return -EINVAL;
    }

    pdev = container_of(dev, struct platform_device, dev);
    hisi_shell = platform_get_drvdata(pdev);

    hisi_shell->channel = channel;
    hisi_shell->debug_temp = temp;

    return count;
}
static DEVICE_ATTR(debug_temp, S_IWUSR, NULL, hisi_shell_store_debug_temp);
#endif

static struct attribute *hisi_shell_attributes[] = {
    &dev_attr_temp.attr,
#ifdef CONFIG_HISI_SHELL_TEMP_DEBUG
    &dev_attr_debug_temp.attr,
#endif
    NULL
};

static struct attribute_group hisi_shell_attribute_group = {
    .attrs = hisi_shell_attributes,
};

static BLOCKING_NOTIFIER_HEAD(ambient_chain_head);
int register_ambient_notifier(struct notifier_block *nb)
{
    return blocking_notifier_chain_register(&ambient_chain_head, nb);
}
EXPORT_SYMBOL_GPL(register_ambient_notifier);

int unregister_ambient_notifier(struct notifier_block *nb)
{
    return blocking_notifier_chain_unregister(&ambient_chain_head, nb);
}
EXPORT_SYMBOL_GPL(unregister_ambient_notifier);

int ambient_notifier_call_chain(int val)
{
    return blocking_notifier_call_chain(&ambient_chain_head, 0, &val);
}

struct temperature_node_t {
    struct device *device;
    int ambient;
};

struct hw_thermal_class {
    struct class *thermal_class;
    struct temperature_node_t temperature_node;
};

static struct hw_thermal_class hw_thermal_info;

#define SHOW_TEMP(temp_name)                                        \
static ssize_t show_##temp_name(struct device *dev, struct device_attribute *attr, char *buf) \
{                                                                   \
    if (!dev || !attr)                                              \
        return 0;                                                   \
    return snprintf(buf, PAGE_SIZE - 1, "%d\n", hw_thermal_info.temperature_node.temp_name); \
}
SHOW_TEMP(ambient);

#define STORE_TEMP(temp_name)                                       \
static ssize_t store_##temp_name(struct device *dev, struct device_attribute *attr, \
                                 const char *buf, size_t count)     \
{                                                                   \
    int temp_name, prev_temp;                                       \
    if (!dev || !attr)                                              \
        return 0;                                                   \
    if (kstrtoint(buf, 10, &temp_name))                             \
        return -EINVAL;                                             \
    prev_temp = hw_thermal_info.temperature_node.temp_name;         \
    hw_thermal_info.temperature_node.temp_name = temp_name;         \
    if (temp_name != prev_temp)                                     \
        ambient_notifier_call_chain(temp_name);                     \
    return count;                                                   \
}
STORE_TEMP(ambient);

static DEVICE_ATTR(ambient, S_IWUSR | S_IRUSR | S_IRGRP | S_IWGRP, show_ambient, store_ambient);

int ipa_get_periph_id(const char *name);
int ipa_get_periph_value(u32 sensor, int *val);
int ipa_get_tsensor_id(const char *name);
int ipa_get_sensor_value(u32 sensor, int *val);

int hisi_get_shell_temp(struct thermal_zone_device *thermal, int *temp)
{
    struct hisi_shell_t *hisi_shell = thermal->devdata;

    if (!hisi_shell || !temp)
        return -EINVAL;

    *temp = hisi_shell->temp;
    return 0;
}

static struct thermal_zone_device_ops shell_thermal_zone_ops = {
    .get_temp = hisi_get_shell_temp,
};

static int calc_shell_temp(struct hisi_shell_t *hisi_shell)
{
    int i, j, k;
    struct hisi_shell_sensor_t *shell_sensor;
    long sum = 0;

    for (i = 0; i < hisi_shell->sensor_count; i++) {
        shell_sensor = get_sensor(hisi_shell, i);
        if (!shell_sensor)
            continue;
        for (j = 0; j < hisi_shell->sample_count; j++) {
            k = (hisi_shell->index - j) < 0 ?
                ((hisi_shell->index - j) + hisi_shell->sample_count) : hisi_shell->index - j;
            sum += (long)shell_sensor->temp_tracing[j].coef * (long)shell_sensor->temp_tracing[k].temp;
            trace_calc_shell_temp(hisi_shell->tz_dev, i, j, shell_sensor->temp_tracing[j].coef,
                                  shell_sensor->temp_tracing[k].temp, sum);
        }
    }

    sum += ((long)hisi_shell->bias * 1000L);
    return (int)(sum / MUTIPLY_FACTOR);
}

static int hkadc_handle_temp_data(struct hisi_shell_t *hisi_shell,
                                  struct hisi_shell_sensor_t *shell_sensor, int temp)
{
    int ret = 0;
    int old_index, old_temp, diff;

    old_index = (hisi_shell->index - 1) < 0 ?
                ((hisi_shell->index - 1) + hisi_shell->sample_count) : hisi_shell->index - 1;
    old_temp = shell_sensor->temp_tracing[old_index].temp;
    diff = (temp - old_temp) < 0 ? (old_temp - temp) : (temp - old_temp);
    diff = (u32)diff / hisi_shell->interval * 1000;

    shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag = 0;
    if (hisi_shell->valid_flag || hisi_shell->index) {
        if ((shell_sensor->sensor_type & CHECK_TSENS) && (diff > hisi_shell->tsensor_temp_step)) {
            pr_err("%s %s sensor_name=%s tsensor_temp_step_out_of_range\n",
                   __func__, hisi_shell->tz_dev->type, shell_sensor->sensor_name);
            shell_sensor->temp_tracing[hisi_shell->index].temp = old_temp;
        } else if ((shell_sensor->sensor_type & CHECK_NTC) && (diff > hisi_shell->ntc_temp_step)) {
            pr_err("%s %s sensor_name=%s ntc_temp_step_out_of_range\n",
                   __func__, hisi_shell->tz_dev->type, shell_sensor->sensor_name);
            shell_sensor->temp_tracing[hisi_shell->index].temp = old_temp;
        }
    }

    if (shell_sensor->sensor_type & CHECK_TSENS) {
        if (temp > hisi_shell->tsensor_max_temp || temp < hisi_shell->tsensor_min_temp) {
            pr_err("%s %s sensor_name=%s sensor_temp=%d tsensor_temp_out_of_range\n",
                   __func__, hisi_shell->tz_dev->type, shell_sensor->sensor_name, temp);
            shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag = INVALID_TEMP_OUT_OF_RANGE;
            ret = HAVE_INVALID_SENSOR_TEMP;
        }
    } else if (shell_sensor->sensor_type & CHECK_NTC) {
        if (temp > hisi_shell->ntc_max_temp || temp < hisi_shell->ntc_min_temp) {
            pr_err("%s %s sensor_name=%s sensor_temp=%d ntc_temp_out_of_range\n",
                   __func__, hisi_shell->tz_dev->type, shell_sensor->sensor_name, temp);
            shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag = INVALID_TEMP_OUT_OF_RANGE;
            ret = HAVE_INVALID_SENSOR_TEMP;
        }
    }

    return ret;
}

static int calc_sensor_temp_avg(struct hisi_shell_t *hisi_shell, int *tsensor_avg, int *ntc_avg)
{
    int i;
    int tsensor_good_count = 0;
    int ntc_good_count = 0;
    int sum_tsensor_temp = 0;
    int sum_ntc_temp = 0;
    int have_tsensor = 0;
    int have_ntc = 0;
    struct hisi_shell_sensor_t *shell_sensor;

    for (i = 0; i < hisi_shell->sensor_count; i++) {
        shell_sensor = get_sensor(hisi_shell, i);
        if (!shell_sensor)
            continue;
        if (shell_sensor->sensor_type & CHECK_TSENS) {
            have_tsensor = 1;
            if (!shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag) {
                tsensor_good_count++;
                sum_tsensor_temp += shell_sensor->temp_tracing[hisi_shell->index].temp;
            }
        }
        if (shell_sensor->sensor_type & CHECK_NTC) {
            have_ntc = 1;
            if (!shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag) {
                ntc_good_count++;
                sum_ntc_temp += shell_sensor->temp_tracing[hisi_shell->index].temp;
            }
        }
    }

    if ((have_tsensor && !tsensor_good_count) || (have_ntc && !ntc_good_count)) {
        pr_err("%s, %s, all tsensors or ntc sensors damaged!\n", __func__, hisi_shell->tz_dev->type);
        return 1;
    }

    if (tsensor_good_count)
        *tsensor_avg = sum_tsensor_temp / tsensor_good_count;
    if (ntc_good_count)
        *ntc_avg = sum_ntc_temp / ntc_good_count;

    return 0;
}

static int handle_invalid_temp(struct hisi_shell_t *hisi_shell)
{
    int i;
    int invalid_temp = ERROR_SHELL_TEMP;
    int tsensor_avg = DEFAULT_SHELL_TEMP;
    int ntc_avg = DEFAULT_SHELL_TEMP;
    struct hisi_shell_sensor_t *shell_sensor;

    if (calc_sensor_temp_avg(hisi_shell, &tsensor_avg, &ntc_avg))
        return 1;

    for (i = 0; i < hisi_shell->sensor_count; i++) {
        shell_sensor = get_sensor(hisi_shell, i);
        if (!shell_sensor)
            continue;
        if ((shell_sensor->sensor_type & CHECK_TSENS) && shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag) {
            invalid_temp = shell_sensor->temp_tracing[hisi_shell->index].temp;
            pr_err("%s, %s, handle tsensor %d invalid data [%d], result is %d\n",
                   __func__, hisi_shell->tz_dev->type, i + 1, invalid_temp, tsensor_avg);
            shell_sensor->temp_tracing[hisi_shell->index].temp = tsensor_avg;
        }
        if ((shell_sensor->sensor_type & CHECK_NTC) && shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag) {
            invalid_temp = shell_sensor->temp_tracing[hisi_shell->index].temp;
            pr_err("%s, %s, handle ntc sensor %d invalid data [%d], result is %d\n",
                   __func__, hisi_shell->tz_dev->type, i + 1, invalid_temp, ntc_avg);
            shell_sensor->temp_tracing[hisi_shell->index].temp = ntc_avg;
        }
        trace_handle_invalid_temp(hisi_shell->tz_dev, i, invalid_temp, shell_sensor->temp_tracing[hisi_shell->index].temp);
    }

    return 0;
}

static int calc_shell_temp_first(struct hisi_shell_t *hisi_shell)
{
    int i;
    int temp = 0;
    int sum_board_sensor_temp = 0;
    int count_board_sensor = 0;
    struct hisi_shell_sensor_t *shell_sensor;

    temp = hisi_battery_temperature() * 1000;
    if ((temp <= hisi_shell->ntc_max_temp) && (temp >= hisi_shell->ntc_min_temp))
        return temp;

    pr_err("%s, %s, battery temperature [%d] out of range!!!\n",
           __func__, hisi_shell->tz_dev->type, temp / 1000);

    for (i = 0; i < hisi_shell->sensor_count; i++) {
        shell_sensor = get_sensor(hisi_shell, i);
        if (!shell_sensor)
            continue;
        if (shell_sensor->sensor_type == TYPE_PERIPHERAL) {
            count_board_sensor++;
            sum_board_sensor_temp += shell_sensor->temp_tracing[hisi_shell->index].temp;
        }
    }

    if (count_board_sensor)
        return (sum_board_sensor_temp / count_board_sensor);
    else {
        pr_err("%s, %s, read temperature of board sensor err!\n", __func__, hisi_shell->tz_dev->type);
        return DEFAULT_SHELL_TEMP;
    }
}

static int set_shell_temp(struct hisi_shell_t *hisi_shell, int have_invalid_temp, int index)
{
    int ret = 0;

    if (have_invalid_temp)
        ret = handle_invalid_temp(hisi_shell);

    if (ret)
        hisi_shell->valid_flag = 0;

    if (!hisi_shell->valid_flag && index >= hisi_shell->sample_count - 1)
        hisi_shell->valid_flag = 1;

    if (hisi_shell->valid_flag && !ret) {
        hisi_shell->temp = calc_shell_temp(hisi_shell);
        if (!hisi_shell->old_temp)
            hisi_shell->old_temp = hisi_shell->temp;
        else {
            int diff = hisi_shell->temp - hisi_shell->old_temp;

            if (diff >= hisi_shell->shell_temp_step_range)
                hisi_shell->temp = hisi_shell->old_temp + hisi_shell->shell_temp_step;
            else if (diff <= -hisi_shell->shell_temp_step_range)
                hisi_shell->temp = hisi_shell->old_temp - hisi_shell->shell_temp_step;
            hisi_shell->old_temp = hisi_shell->temp;
        }
    } else if (!hisi_shell->valid_flag && !ret) {
        hisi_shell->temp = calc_shell_temp_first(hisi_shell);
        hisi_shell->old_temp = 0;
    } else {
        hisi_shell->temp = ERROR_SHELL_TEMP;
        hisi_shell->old_temp = 0;
    }

    trace_shell_temp(hisi_shell->tz_dev, hisi_shell->temp);
    return ret;
}

static void hkadc_sample_temp(struct work_struct *work)
{
    int i, index, ret;
    int have_invalid_temp = 0;
    struct hisi_shell_t *hisi_shell;
    struct hisi_shell_sensor_t *shell_sensor;
    struct thermal_zone_device *tz;
    int temp = 0;

    hisi_shell = container_of((struct delayed_work *)work, struct hisi_shell_t, work);
    index = hisi_shell->index;
    mod_delayed_work(system_freezable_power_efficient_wq, &hisi_shell->work,
                     round_jiffies(msecs_to_jiffies(hisi_shell->interval)));

    for (i = 0; i < hisi_shell->sensor_count; i++) {
        shell_sensor = get_sensor(hisi_shell, i);
        if (!shell_sensor)
            continue;

        tz = thermal_zone_get_zone_by_name(shell_sensor->sensor_name);
        if (IS_ERR(tz)) {
            pr_err("failed to get thermal zone %s\n", shell_sensor->sensor_name);
            temp = DEFAULT_SHELL_TEMP;
        } else {
            ret = thermal_zone_get_temp(tz, &temp);
            if (shell_sensor->sensor_type == TYPE_TSENS)
                temp = temp * 1000;
            if (ret)
                temp = DEFAULT_SHELL_TEMP;
            put_thermal_zone(tz);
        }

#ifdef CONFIG_HISI_SHELL_TEMP_DEBUG
        if ((i == hisi_shell->channel - 1) && (hisi_shell->debug_temp)) {
            pr_err("%s, %s, sensor_name=%s, temp=%d\n", __func__, hisi_shell->tz_dev->type,
                   shell_sensor->sensor_name, hisi_shell->debug_temp);
            temp = hisi_shell->debug_temp;
        }
#endif

        shell_sensor->temp_tracing[hisi_shell->index].temp = temp;
        have_invalid_temp = hkadc_handle_temp_data(hisi_shell, shell_sensor, (int)temp);
    }

    if (!set_shell_temp(hisi_shell, have_invalid_temp, index))
        index++;
    else
        index = 0;
    hisi_shell->index = index >= hisi_shell->sample_count ? 0 : index;
}

static int fill_sensor_coef(struct hisi_shell_t *hisi_shell, struct device_node *np)
{
    int i = 0, j = 0, coef = 0, ret = 0;
    const char *ptr_coef = NULL, *ptr_type = NULL;
    struct device_node *child;
    struct hisi_shell_sensor_t *shell_sensor;
    struct thermal_zone_device *tz = NULL;

    for_each_child_of_node(np, child) {
        if (i >= hisi_shell->sensor_count)
            break;

        shell_sensor = get_sensor(hisi_shell, i);
        if (!shell_sensor) {
            ret = -EINVAL;
            break;
        }

        ret = of_property_read_string(child, "type", &ptr_type);
        if (ret) {
            pr_err("%s, %s, type read err\n", __func__, hisi_shell->tz_dev->type);
            return ret;
        }

        if ((ret = ipa_get_tsensor_id(ptr_type)) >= 0) {
            shell_sensor->sensor_type = TYPE_TSENS;
        } else if ((ret = ipa_get_periph_id(ptr_type)) >= 0) {
            shell_sensor->sensor_type = TYPE_PERIPHERAL;
        } else if (!strncmp(ptr_type, BATTERY_NAME, strlen(BATTERY_NAME))) {
            pr_info("%s, %s, type is Battery.\n", __func__, hisi_shell->tz_dev->type);
            shell_sensor->sensor_type = TYPE_BATTERY;
        } else if (!IS_ERR(tz = thermal_zone_get_zone_by_name(ptr_type))) {
            pr_info("%s, %s, terminal sensor\n", __func__, hisi_shell->tz_dev->type);
            shell_sensor->sensor_type = TYPE_TERMINAL;
            put_thermal_zone(tz);
        } else {
            pr_err("%s, %s, %s, sensor id get err\n", __func__, hisi_shell->tz_dev->type, ptr_type);
            shell_sensor->sensor_type = TYPE_UNKNOWN;
        }
        shell_sensor->sensor_name = ptr_type;

        for (j = 0; j < hisi_shell->sample_count; j++) {
            ret = of_property_read_string_index(child, "coef", j, &ptr_coef);
            if (ret) {
                pr_err("%s coef [%d] read err\n", __func__, j);
                return ret;
            }

            ret = kstrtoint(ptr_coef, 10, &coef);
            if (ret) {
                pr_err("%s kstortoint is failed\n", __func__);
                return ret;
            }
            shell_sensor->temp_tracing[j].coef = coef;
        }

        i++;
    }

    return 0;
}

static int read_temp_range(struct device_node *dev_node, struct hisi_shell_t *hisi_shell,
                           int *tsensor_range, int *ntc_range)
{
    int ret = 0;
    const char *ptr_tsensor = NULL, *ptr_ntc = NULL;
    int i;

    ret = of_property_read_s32(dev_node, "shell_temp_step_range", &hisi_shell->shell_temp_step_range);
    if (ret) {
        pr_err("%s shell_temp_step_range read err\n", __func__);
        hisi_shell->shell_temp_step_range = DEFAULT_SHELL_TEMP_STEP_RANGE;
    }

    ret = of_property_read_s32(dev_node, "shell_temp_step", &hisi_shell->shell_temp_step);
    if (ret) {
        pr_err("%s shell_temp_step read err\n", __func__);
        hisi_shell->shell_temp_step = DEFAULT_SHELL_TEMP_STEP;
    }

    for (i = 0; i < SENSOR_PARA_COUNT; i++) {
        ret = of_property_read_string_index(dev_node, "tsensor_para", i, &ptr_tsensor);
        if (ret) {
            pr_err("%s tsensor_para[%d] read err\n", __func__, i);
            return -EINVAL;
        }

        ret = kstrtoint(ptr_tsensor, 10, &tsensor_range[i]);
        if (ret) {
            pr_err("%s kstrtoint is failed with tsensor_para[%d]\n", __func__, i);
            return -EINVAL;
        }

        ret = of_property_read_string_index(dev_node, "ntc_para", i, &ptr_ntc);
        if (ret) {
            pr_err("%s ntc_para[%d] read err\n", __func__, i);
            return -EINVAL;
        }

        ret = kstrtoint(ptr_ntc, 10, &ntc_range[i]);
        if (ret) {
            pr_err("%s kstrtoint is failed with ntc_para[%d]", __func__, i);
            return -EINVAL;
        }
    }

    return 0;
}

static int hisi_shell_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct device_node *dev_node = dev->of_node;
    int ret;
    int sample_count, sensor_count;
    int tsensor_para[SENSOR_PARA_COUNT], ntc_para[SENSOR_PARA_COUNT];
    struct device_node *np;
    struct hisi_shell_t *hisi_shell;
    size_t alloc_size;
    char *ptr;
    int i;

    if (!of_device_is_available(dev_node)) {
        dev_err(dev, "HISI shell dev not found\n");
        return -ENODEV;
    }

    ret = of_property_read_s32(dev_node, "count", &sample_count);
    if (ret) {
        pr_err("%s count read err\n", __func__);
        goto exit;
    }

    np = of_find_node_by_name(dev_node, "sensors");
    if (!np) {
        pr_err("sensors node not found\n");
        ret = -ENODEV;
        goto exit;
    }

    sensor_count = of_get_child_count(np);
    if (sensor_count <= 0) {
        ret = -EINVAL;
        pr_err("%s sensor count read err\n", __func__);
        goto node_put;
    }

    alloc_size = sizeof(*hisi_shell) + sensor_count * sizeof(struct hisi_shell_sensor_t *);
    for (i = 0; i < sensor_count; i++)
        alloc_size += sensor_size(sample_count);

    hisi_shell = kzalloc(alloc_size, GFP_KERNEL);
    if (!hisi_shell) {
        ret = -ENOMEM;
        pr_err("no enough memory\n");
        goto node_put;
    }

    ptr = (char *)hisi_shell + sizeof(*hisi_shell) + sensor_count * sizeof(struct hisi_shell_sensor_t *);
    for (i = 0; i < sensor_count; i++) {
        hisi_shell->sensors[i] = (struct hisi_shell_sensor_t *)ptr;
        ptr += sensor_size(sample_count);
    }

    hisi_shell->sensor_count = sensor_count;
    hisi_shell->sample_count = sample_count;

    if (read_temp_range(dev_node, hisi_shell, tsensor_para, ntc_para)) {
        hisi_shell->tsensor_temp_step = DEFAULT_TSENS_STEP_RANGE;
        hisi_shell->tsensor_max_temp = DEFAULT_TSENS_MAX_TEMP;
        hisi_shell->tsensor_min_temp = DEFAULT_TSENS_MIN_TEMP;
        hisi_shell->ntc_temp_step = DEFAULT_NTC_STEP_RANGE;
        hisi_shell->ntc_max_temp = DEFAULT_NTC_MAX_TEMP;
        hisi_shell->ntc_min_temp = DEFAULT_NTC_MIN_TEMP;
    } else {
        hisi_shell->tsensor_temp_step = tsensor_para[0];
        hisi_shell->tsensor_max_temp = tsensor_para[1];
        hisi_shell->tsensor_min_temp = tsensor_para[2];
        hisi_shell->ntc_temp_step = ntc_para[0];
        hisi_shell->ntc_max_temp = ntc_para[1];
        hisi_shell->ntc_min_temp = ntc_para[2];
    }

    hisi_shell->index = 0;
    hisi_shell->valid_flag = 0;
    hisi_shell->old_temp = 0;
#ifdef CONFIG_HISI_SHELL_TEMP_DEBUG
    hisi_shell->channel = 0;
    hisi_shell->debug_temp = 0;
#endif

    ret = of_property_read_u32(dev_node, "interval", &hisi_shell->interval);
    if (ret) {
        pr_err("%s interval read err\n", __func__);
        goto free_mem;
    }

    ret = of_property_read_s32(dev_node, "bias", &hisi_shell->bias);
    if (ret) {
        pr_err("%s bias read err\n", __func__);
        goto free_mem;
    }

    ret = fill_sensor_coef(hisi_shell, np);
    if (ret)
        goto free_mem;

    hisi_shell->tz_dev = thermal_zone_device_register(dev_node->name, 0, 0, hisi_shell,
                                                      &shell_thermal_zone_ops, NULL, 0, 0);
    if (IS_ERR(hisi_shell->tz_dev)) {
        dev_err(dev, "register thermal zone for shell failed.\n");
        ret = -ENODEV;
        goto free_mem;
    }

    hisi_shell->temp = hisi_battery_temperature() * 1000;
    pr_info("%s: temp %d\n", dev_node->name, hisi_shell->temp);

    platform_set_drvdata(pdev, hisi_shell);

    INIT_DELAYED_WORK(&hisi_shell->work, hkadc_sample_temp);
    mod_delayed_work(system_freezable_power_efficient_wq, &hisi_shell->work,
                     round_jiffies(msecs_to_jiffies(hisi_shell->interval)));

    ret = sysfs_create_link(&hw_thermal_info.temperature_node.device->kobj, &pdev->dev.kobj, dev_node->name);
    if (ret) {
        pr_err("%s: create hw_thermal device file error: %d\n", dev_node->name, ret);
        goto cancel_work;
    }

    ret = sysfs_create_group(&pdev->dev.kobj, &hisi_shell_attribute_group);
    if (ret) {
        pr_err("%s: create shell file error: %d\n", dev_node->name, ret);
        sysfs_remove_link(&hw_thermal_info.temperature_node.device->kobj, dev_node->name);
        goto cancel_work;
    }

    of_node_put(np);
    return 0;

cancel_work:
    cancel_delayed_work_sync(&hisi_shell->work);
    thermal_zone_device_unregister(hisi_shell->tz_dev);
free_mem:
    kfree(hisi_shell);
node_put:
    of_node_put(np);
exit:
    return ret;
}

static int hisi_shell_remove(struct platform_device *pdev)
{
    struct hisi_shell_t *hisi_shell = platform_get_drvdata(pdev);

    if (hisi_shell) {
        cancel_delayed_work_sync(&hisi_shell->work);
        sysfs_remove_link(&hw_thermal_info.temperature_node.device->kobj, pdev->dev.of_node->name);
        sysfs_remove_group(&pdev->dev.kobj, &hisi_shell_attribute_group);
        thermal_zone_device_unregister(hisi_shell->tz_dev);
        kfree(hisi_shell);
    }

    return 0;
}

int shell_temp_pm_resume(struct platform_device *pdev)
{
    struct hisi_shell_t *hisi_shell;

    pr_info("%s+\n", __func__);
    hisi_shell = platform_get_drvdata(pdev);

    if (hisi_shell) {
        hisi_shell->temp = hisi_battery_temperature() * 1000;
        pr_info("%s: temp %d\n", hisi_shell->tz_dev->type, hisi_shell->temp);
        hisi_shell->index = 0;
        hisi_shell->valid_flag = 0;
        hisi_shell->old_temp = 0;
#ifdef CONFIG_HISI_SHELL_TEMP_DEBUG
        hisi_shell->channel = 0;
        hisi_shell->debug_temp = 0;
#endif
    }
    pr_info("%s-\n", __func__);
    return 0;
}

static const struct of_device_id hisi_shell_of_match[] = {
    { .compatible = "hisi,shell-temp" },
    {},
};
MODULE_DEVICE_TABLE(of, hisi_shell_of_match);

static struct platform_driver hisi_shell_platdrv = {
    .driver = {
        .name           = "hisi-shell-temp",
        .owner          = THIS_MODULE,
        .of_match_table = hisi_shell_of_match,
    },
    .probe  = hisi_shell_probe,
    .remove = hisi_shell_remove,
    .resume = shell_temp_pm_resume,
};

static int __init hisi_shell_init(void)
{
    int ret = 0;

    hw_thermal_info.thermal_class = class_create(THIS_MODULE, "hw_thermal");
    if (IS_ERR(hw_thermal_info.thermal_class)) {
        pr_err("Huawei thermal class create error\n");
        return PTR_ERR(hw_thermal_info.thermal_class);
    }

    hw_thermal_info.temperature_node.device = device_create(hw_thermal_info.thermal_class, NULL, 0, NULL, "temp");
    if (IS_ERR(hw_thermal_info.temperature_node.device)) {
        pr_err("hw_thermal:temperature_node device create error\n");
        class_destroy(hw_thermal_info.thermal_class);
        hw_thermal_info.thermal_class = NULL;
        return PTR_ERR(hw_thermal_info.temperature_node.device);
    }

    ret = device_create_file(hw_thermal_info.temperature_node.device, &dev_attr_ambient);
    if (ret) {
        pr_err("hw_thermal:ambient node create error\n");
        device_destroy(hw_thermal_info.thermal_class, 0);
        class_destroy(hw_thermal_info.thermal_class);
        hw_thermal_info.thermal_class = NULL;
        return ret;
    }

    return platform_driver_register(&hisi_shell_platdrv);
}

static void __exit hisi_shell_exit(void)
{
    if (hw_thermal_info.thermal_class) {
        device_destroy(hw_thermal_info.thermal_class, 0);
        class_destroy(hw_thermal_info.thermal_class);
    }
    platform_driver_unregister(&hisi_shell_platdrv);
}

module_init(hisi_shell_init);
module_exit(hisi_shell_exit);

MODULE_LICENSE("GPL v2");