#include <gtk/gtk.h>
#include <spice-client-gtk.h>

GtkWidget *window;
SpiceSession* spiceSession;


void onClose() {
	spice_session_disconnect(spiceSession);
}

void channelEvent(SpiceChannel *channel, SpiceChannelEvent event, gpointer user_data) {
	if (event == SPICE_CHANNEL_ERROR_CONNECT) {
		printf("SPICE_CHANNEL_ERROR_CONNECT\n");
		g_signal_emit_by_name(window, "destroy");
	}
}

void newChannel(SpiceSession *session, SpiceChannel *channel, gpointer user_data) {
	gint channelType; 
	g_object_get(channel, "channel-type", &channelType, NULL);
	printf("%d: %s\n", channelType, spice_channel_type_to_string(channelType));
	
	if (channelType == 1) {
		g_signal_connect(G_OBJECT(channel), "channel-event", G_CALLBACK(channelEvent), NULL);
	} else if (channelType == 2) {
		gint channelId; 
		g_object_get(channel, "channel-id", &channelId, NULL);
		SpiceDisplay* spiceDisplay = spice_display_new(session, channelId);

		gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(spiceDisplay));
		gtk_widget_show_all(window);
		
		GtkWidget* USBWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(USBWindow), "USB redirection");
		GtkWidget* USBWidget = spice_usb_device_widget_new(session, "%s %s %s at %d-%d");
		gtk_container_add(GTK_CONTAINER(USBWindow), GTK_WIDGET(USBWidget));
		gtk_widget_show_all(USBWindow);
	}
}


void activate (GtkApplication *app, gpointer user_data) {
	window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(window), "VM Viewer");
	gtk_window_set_default_size(GTK_WINDOW (window), 800, 600);
	g_signal_connect(G_OBJECT(window), "destroy", onClose, NULL);
	
	spiceSession = spice_session_new();
	
	g_signal_connect(G_OBJECT(spiceSession), "channel-new", G_CALLBACK(newChannel), NULL);
	
	GValue uri = G_VALUE_INIT;
	g_value_init(&uri, G_TYPE_STRING);
	g_value_set_static_string(&uri, "spice://localhost?port=5900");
	g_object_set_property(G_OBJECT(spiceSession), "uri", &uri);
	g_value_unset(&uri);
	spice_session_connect(spiceSession);
}

int main (int argc, char **argv) {
	GtkApplication *app;
	int status;

	app = gtk_application_new("org.gtk.example", G_APPLICATION_FLAGS_NONE);
	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
	status = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);

	return status;
}

