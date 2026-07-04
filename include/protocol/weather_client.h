#ifndef PROTOCOL_WEATHER_CLIENT_H
#define PROTOCOL_WEATHER_CLIENT_H

int weather_client_fetch(const char *wsl_ip, const char *get_weather_script, char *result_wuhan, int wuhan_len, char *result_beijing, int beijing_len);
void weather_client_get_city_names(char *city1, int city1_len, char *city2, int city2_len);
void weather_client_get_update_time(char *update_time, int update_time_len);

#endif // PROTOCOL_WEATHER_CLIENT_H

