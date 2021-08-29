# Create a Virtual Machine viewer with Spice-GTK



One way to work with Linux virtual machines (VM) is to use [Libvirt](https://libvirt.org/) to manage the VM, and [Spice](https://www.spice-space.org/) to interact with the VM. The [Libvirt API](https://libvirt.org/html/index.html) and the [Libvirt Application Development Guide](https://libvirt.org/docs/libvirt-appdev-guide-python/en-US/html/) show how to create an application with Libvirt. Such guide not being present for Spice, I will show in this tutorial how to create a GTK application in C for interacting and controlling a VM.

To follow this tutorial, you will need:

* The Fedora distribution (it should work on other distro but you will have to change the `dnf` commands).
* Some basic knowledge in C and GTK.

## 1. Creating a VM

The first step is to create a VM running a Spice server. I recommend using the [virt-manager](https://virt-manager.org/) software. Plenty of tutorials showing how to install virt-manager and create a VM are available on the Web, so I will not detail this part.

By default, a Spice server is used when creating a VM with virt-manager, but you can check it by opening the VM, going to the `Details` view and on the `Display Spice` tab. Here, you should choose the `Type` as `Spice server`. If the VM is running, note the port used as we will need it later (by default, it should be 5900).

## 2. Installing dependencies

To develop this application, we need both the development files of GTK3 (GTK4 is not supported by Spice-GTK) and Spice-GTK.

GTK3 can be installed with:

```shell
sudo dnf install gtk3-devel
```

And Spice-GTK:

```shell
sudo dnf builddep spice-gtk
wget https://www.spice-space.org/download/gtk/spice-gtk-0.39.tar.xz
tar -xf spice-gtk-0.39.tar.xz
cd spice-gtk-0.39/
meson builddir && cd builddir
meson compile
sudo meson install
```

## 3. Creating the application

Let's now create the application. The final source code is available on [Github](https://github.com/Rylern/SpiceGTK-tutorial).

We can begin with a basic window and include GTK and Spice-GTK to make sure everything is installed properly:

```c
#include <gtk/gtk.h>
#include <spice-client-gtk.h>

void activate (GtkApplication *app, gpointer user_data) {
	GtkWidget* window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(window), "VM Viewer");
	gtk_window_set_default_size(GTK_WINDOW (window), 800, 600);
	gtk_widget_show_all(window);
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
```

To compile, we need both `gtk+-3.0` and `spice-client-gtk-3.0`:

```shell
gcc main.c `pkg-config --cflags gtk+-3.0 spice-client-gtk-3.0` `pkg-config --libs gtk+-3.0 spice-client-gtk-3.0`
```

We can now start using the Spice library. The API reference is available [here](https://www.spice-space.org/spice-gtk.html).

The first thing to do is to create a Spice session, pass the connection information to it and connecting to the VM:

```c
#include <gtk/gtk.h>
#include <spice-client-gtk.h>

SpiceSession* spiceSession;

void onClose() {
    // Close the Spice session
	spice_session_disconnect(spiceSession);
}

void activate (GtkApplication *app, gpointer user_data) {
	GtkWidget *window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(window), "VM Viewer");
	gtk_window_set_default_size(GTK_WINDOW (window), 800, 600);
	g_signal_connect(G_OBJECT(window), "destroy", onClose, NULL);
	gtk_widget_show_all(window);
	
    // Create a Spice session
	spiceSession = spice_session_new();
    
    // Pass the URI of the VM to the session
	GValue uri = G_VALUE_INIT;
	g_value_init(&uri, G_TYPE_STRING);
	g_value_set_static_string(&uri, "spice://localhost?port=5900");
	g_object_set_property(G_OBJECT(spiceSession), "uri", &uri);
	g_value_unset(&uri);
    
    // Connect to the VM
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
```

The URI is of the form `spice://hostname?port=XXXX`. Here, we connect to a local machine but it could also be a distant machine (if the firewall of the distant host permits it). The port can be found by opening the VM with Virt-manager, going to the `Details` view and on the `Display Spice` tab. By default, it should be 5900. It is also possible to access the port with Libvirt. The session must be closed before leaving the application, which is done here by the `onClose()` function connected to the `destroy` signal of the window.

When a spice session is connecting, "channels" are created. There is one channel for each type of interaction, for example:

* The Main channel, which handles communication initialization (channels list), migrations, mouse modes, multimedia time, and agent communication.
* The Display channel, which renders the remote display.
* The USB Redirection channel, which can redirect USB devices from the host to the VM.
* The Cursor channel, which updates the cursor shape and position.
* The Inputs channel, which control the server mouse and keyboard.
* And more described in the [API reference](https://www.spice-space.org/spice-gtk.html).

The first channel we are going to use is the Main channel, because it can tell us whether the connection to the VM was a success or not. To do this, we have to connect the session to the `channel-new` signal. This signal will emit each time a channel is created. Once the Main channel is created, we have to connect it to the `channel-event` signal. This signal will indicate whether the session successfully connected to the VM.

```c
#include <gtk/gtk.h>
#include <spice-client-gtk.h>

GtkWidget *window;
SpiceSession* spiceSession;

void onClose() {
	spice_session_disconnect(spiceSession);
}

// Called when an event happens to the Main channel
void channelEvent(SpiceChannel *channel, SpiceChannelEvent event, gpointer user_data) {
    // If there is a connection error, we end the application
	if (event == SPICE_CHANNEL_ERROR_CONNECT) {
		printf("SPICE_CHANNEL_ERROR_CONNECT\n");
		g_signal_emit_by_name(window, "destroy");
	}
}

// Called when a new channel is created
void newChannel(SpiceSession *session, SpiceChannel *channel, gpointer user_data) {
   	// Get the channel type
	gint channelType; 
	g_object_get(channel, "channel-type", &channelType, NULL);
	printf("%d: %s\n", channelType, spice_channel_type_to_string(channelType));
	
    // Type 1 corresponds to the Main channel
	if (channelType == 1) {
        // Connect the Main channel to the channel-event signal
		g_signal_connect(G_OBJECT(channel), "channel-event", G_CALLBACK(channelEvent), NULL);
	}
}

void activate (GtkApplication *app, gpointer user_data) {
	window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(window), "VM Viewer");
	gtk_window_set_default_size(GTK_WINDOW (window), 800, 600);
	g_signal_connect(G_OBJECT(window), "destroy", onClose, NULL);
	gtk_widget_show_all(window);
	
	spiceSession = spice_session_new();
	
    // Connect the session to the channel-new signal
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
```

The `newChannel()` function is called each time a channel is created. We can get the type of the channel (main, display, USB redirection...) by looking at its `channel-type` property. This property is an enum (so a number), but we can get its corresponding string with the `spice_channel_type_to_string()` function. For example, the enum `1` corresponds to the Main channel.

The `channelEvent()` function is called each time the state of the main channel changes. We use it here to detect any connection error.

We can now display the screen of the VM. We will use a GTK display widget provided by Spice. It requires the Display channel, so it will be created in the `newChannel()` function:

```c
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
	}
    // Type 2 corresponds to the Display channel
    else if (channelType == 2) {
        // Get the channel ID of the Display channel
		gint channelId; 
		g_object_get(channel, "channel-id", &channelId, NULL);
        
        // Create the display widget
		SpiceDisplay* spiceDisplay = spice_display_new(session, channelId);

        // Add the display widget to the main window 
		gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(spiceDisplay));
		gtk_widget_show_all(window);
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
```

To create the display widget, we need the `channel-id` property of the Display channel.

The Inputs channel will automatically redirects the mouse and keyboard to the VM, and the Cursor channel will automatically updates the cursor shape and position, so we have now a fully working viewer.

Finally, I will show how to redirect USB devices to the VM. Spice provides a widget for this, the Spice USB device selection widget. I will put it inside a new window and display it when the display widget is launched:

```c
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
		
        // Create a window
		GtkWidget* USBWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(USBWindow), "USB redirection");
        
        // Put the USB widget inside the new window and display it
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
```

The second argument of the `spice_usb_device_widget_new()` function is a string describing each device inside the Spice USB device selection widget, with the following format:

* First `%s`: manufacturer.
* Second `%s`: product.
* Third `%s`: descriptor (a [vendor_id:product_id] string).
* First `%d`: bus.
* Second `%d`: address.

## 4. Conclusion

You can now create a simple VM viewer with Spice and GTK. More advanced options are described in the [API reference](https://www.spice-space.org/spice-gtk.html). 

To create an application able to fully manage and interact with virtual machines, I recommend using the [Libvirt API](https://libvirt.org/html/index.html) along with Spice-GTK.
