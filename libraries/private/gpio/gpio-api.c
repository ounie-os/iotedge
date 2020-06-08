#include "gpio-api.h"

#define GPIO_EXPORT_PATH "/sys/class/gpio/export"
#define GPIO_UNEXPORT_PATH "/sys/class/gpio/unexport"
#define GPIO_SYS_PATH "/sys/class/gpio"

/**
 * 
 * @brief 
 * 
 * @param[in]   @n 
 * @param[out]  @n 
 * @return 
 */
static int _gpio_init(const char *path)
{
	int fd = open(path, O_WRONLY);
	if (fd == -1)
	{
		printf("open <%s> fail\n", path);
	}
	return fd;
}

/**
 * 
 * @brief 
 * 
 * @param[in]   @n 
 * @param[out]  @n 
 * @return 
 */
int gpio_request(int pin)
{
	int ret = 0;
	char buf[8] = {0};
	int fd = _gpio_init(GPIO_EXPORT_PATH);
	if (fd > 0)
	{
		snprintf(buf, 7, "%d", pin);
		ret = write(fd, (const void *)&buf, strlen(buf));
		if (ret == -1)
		{
			printf("export gpio <%d> fail\n", pin);
			close(fd);
			return -1;
		}
		close(fd);
		return 0;
	}
	else
	{
		return -1;
	}
}

/**
 * 
 * @brief 
 * 
 * @param[in]   @n 
 * @param[in]  @n 
 * @return -1:fail
 */
int gpio_set_direction(int pin, int dir)
{
	char path_buf[128] = {0};
	const char *dir_str[2] = {"in", "out"};
	int fd;
	int ret = -1;
	snprintf(path_buf, 127, "%s/gpio%d/direction", GPIO_SYS_PATH, pin);
	fd = open(path_buf, O_WRONLY);
	if (fd)
	{
		printf("%s - path = %s, dir = %d, %s\n", __func__, path_buf, dir, dir_str[dir]);
		switch (dir)
		{
			case 0:
				ret = write(fd, dir_str[0], strlen(dir_str[0]));
				break;
			case 1:
				ret = write(fd, dir_str[1], strlen(dir_str[1]));
				break;
			default:
				break;
		}
		if (ret == -1)
		{
			printf("write direction error\n");
		}
		close(fd);
		return ret;
	}
	else
	{
		return ret;
	}
}

/**
 * 
 * @brief 
 * 
 * @param[in]   @n 
 * @param[out]  @n 
 * @return 
 */
int gpio_put_status(int pin, int status)
{
	char path_buf[128] = {0};
	int fd;
	const char *gpio_value[2] = {"0", "1"};
	const char *buf;
	snprintf(path_buf, 127, "%s/gpio%d/value", GPIO_SYS_PATH, pin);
	fd = open(path_buf, O_WRONLY);
	if (fd)
	{
		buf = ((status == 0) ? gpio_value[0] : gpio_value[1]);
		write(fd, buf, 1);
		close(fd);
		return 0;
	}
	else
	{
		return -1;
	}
}

/**
 * 
 * @brief 
 * 
 * @param[in]   @n 
 * @param[out]  @n 
 * @return 
 */
int gpio_get_status(int pin)
{
    int fd;
    char path_buf[128] = {0};
    char gpio_value[8] = {0};
    snprintf(path_buf, 127, "%s/gpio%d/value", GPIO_SYS_PATH, pin);
    fd = open(path_buf, O_RDONLY);
    if (fd)
    {
        read(fd, gpio_value, sizeof(gpio_value) - 1);
        close(fd);
        return (int)(gpio_value[0] - '0');
    }
    else
    {
        return -1;
    }
    
}

/**
 * 
 * @brief 
 * 
 * @param[in]   @n 
 * @param[out]  @n 
 * @return 
 */
int gpio_free(int pin)
{
	char buf[8] = {0};
	int fd = open(GPIO_UNEXPORT_PATH, O_WRONLY);
	if (fd)
	{
		snprintf(buf, 7, "%d", pin);
		write(fd, buf, strlen(buf));
		close(fd);
	}
	return 0;
}

#if 0
int main(int argc, char **argv)
{
	int pin, dir, status;
	printf("%s, %s, %s\n", argv[1], argv[2], argv[3]);
	pin = atoi(argv[1]);
	dir = atoi(argv[2]);
	status = atoi(argv[3]);
	printf("pin=%d, dir=%d, status=%d\n", pin, dir, status);
	if ( -1 != gpio_request(pin))
	{
		if ( -1 != gpio_set_direction(pin, dir))
		{
			gpio_put_status(pin, status);
			sleep(3);
			gpio_free(pin);
		}
	}
	return 0;
}
#endif /* 0 */

