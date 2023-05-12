#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <gtk/gtk.h>
#include <gtk/gtkcssprovider.h> 
#include <glib.h>
//  gcc pc.c -o pc `pkg-config --cflags --libs gtk+-3.0` -lpthread -g -fsanitize=address
//#define GDK_VERSION_MIN_REQUIRED GDK_VERSION_3_22
#define MAX_STOCK 100
#define PRODUCER_INCREMENT 1
#define CONSUMER_DECREMENT 1
static GMutex mutex;
GCond not_empty;
GCond not_full;


typedef struct {
    GdkRGBA color;
    GtkWidget*  window;
    GtkWidget*  box;
    GtkWidget*  producer_container;
    GtkWidget*  status_container;
    GtkWidget*  consumer_container;
    GtkWidget*  plus_producer;
    GtkWidget*  txt_producer;
    GtkWidget*  less_producer;
    GtkWidget*  plus_consumer;
    GtkWidget*  txt_consumer;
    GtkWidget*  less_consumer;
    GtkWidget*  bar_status;
    GtkWidget*  txt_status;
} app_widgets;
typedef app_widgets* ptr_app_widgets;

enum roles{
    CONSUMER,
    PRODUCER,
    EMPTY
};
struct person {
    pthread_t th_id;

    struct person* next_person; //puntero a otro elemento
};
typedef struct person person;
typedef person* ptr_person;

typedef struct {
    float current_stock;
    int current_producers;
    int current_consumers;

    ptr_person first_stack_consumer;
    ptr_person first_stack_producer;

    pthread_cond_t not_full;
    pthread_cond_t not_empty;
    pthread_mutex_t lock;
} stack;
typedef stack* ptr_stack;

typedef struct {
    ptr_stack my_stack;
    ptr_app_widgets screen;
    enum roles role;
}callback_data;
typedef callback_data* ptr_callback_data;

//Visual methods
void init_app_widgets(ptr_app_widgets, ptr_stack, ptr_callback_data);
static gboolean on_delete_event(GtkWidget*, gpointer);
void push_producer(GtkWidget*, gpointer);
void pop_producer(GtkWidget*, gpointer);
void push_consumer(GtkWidget*, gpointer);
void pop_consumer(GtkWidget*, gpointer);
gboolean update_bar(void *);

//Program methods
void init_stack(ptr_stack);
void* consumir (void *);
void* producir (void *);
void cleanup_handler(void*);


int main(int argc, char *argv[]){
    stack my_stack;
    app_widgets WIDGETS;
    callback_data data;
    pthread_t p1;
    pthread_t c1;

    
    printf("Versión de GTK+: %d.%d.%d\n",GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);

    //Variables de widgets
    
    //Init
    gtk_init(NULL, NULL);
    init_stack(&my_stack);
    init_app_widgets(&WIDGETS, &my_stack, &data);
    


    //Señales conectadas a su funcion callback
    g_idle_add(update_bar, &data);

    //Muestra el widget principal en pantalla
    gtk_widget_show_all(WIDGETS.window);

    //gdk_threads_init();
    //gdk_threads_enter();
    //Ciclo principal que atiende a todos los eventos durante todo el ciclo de vida del programa
    gtk_main();
    //gdk_threads_leave();
    //Crear hilos
    
    


    return 0;
    
}

void init_app_widgets(app_widgets* WIDGETS, stack* my_stack, ptr_callback_data my_data){
    my_data->my_stack = my_stack;
    my_data->screen = WIDGETS;
    char txt_number[20]; //variable auxiliar para parsear enteros a char[]


    WIDGETS->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(WIDGETS->window),"Productor-Consumidor");
    gtk_window_set_default_size(GTK_WINDOW(WIDGETS->window),400,300);
    gtk_window_set_resizable(GTK_WINDOW(WIDGETS->window),FALSE);
    //gdk_rgba_parse(&color, "blue");
    //gtk_widget_set_css_color(window, "background-color", &color);
    g_signal_connect(WIDGETS->window, "delete-event", G_CALLBACK(on_delete_event), NULL);

    
    gdk_rgba_parse(&WIDGETS->color, "#b4ffb4");
    WIDGETS->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(WIDGETS->window), WIDGETS->box);
    gtk_widget_override_background_color(GTK_WIDGET(WIDGETS->box), GTK_STATE_FLAG_NORMAL, &WIDGETS->color);

    

    WIDGETS->producer_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(WIDGETS->box), WIDGETS->producer_container);
    gtk_container_set_border_width(GTK_CONTAINER(WIDGETS->producer_container),10);


    WIDGETS->status_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(WIDGETS->box), WIDGETS->status_container);
    gtk_container_set_border_width(GTK_CONTAINER(WIDGETS->status_container),10);
    
    
    WIDGETS->consumer_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(WIDGETS->box), WIDGETS->consumer_container);
    gtk_container_set_border_width(GTK_CONTAINER(WIDGETS->consumer_container),10);





    WIDGETS->plus_producer = gtk_button_new_with_label("+");
    g_signal_connect(WIDGETS->plus_producer, "clicked", G_CALLBACK(push_producer), my_data);
    gtk_container_add(GTK_CONTAINER(WIDGETS->producer_container), WIDGETS->plus_producer);


    sprintf(txt_number ,"%d",my_stack->current_producers);
    WIDGETS->txt_producer = gtk_label_new(txt_number);
    gtk_container_add(GTK_CONTAINER(WIDGETS->producer_container), WIDGETS->txt_producer);


    WIDGETS->less_producer = gtk_button_new_with_label("-");
    g_signal_connect(WIDGETS->less_producer, "clicked", G_CALLBACK(pop_producer), my_data);
    gtk_container_add(GTK_CONTAINER(WIDGETS->producer_container), WIDGETS->less_producer);


    sprintf(txt_number ,"%d",my_stack->current_stock);
    WIDGETS->txt_status = gtk_label_new(txt_number);
    gtk_container_add(GTK_CONTAINER(WIDGETS->status_container), WIDGETS->txt_status);

    WIDGETS->bar_status = gtk_progress_bar_new();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(WIDGETS->bar_status), 0.0);
    gtk_container_add(GTK_CONTAINER(WIDGETS->status_container), WIDGETS->bar_status);


    WIDGETS->plus_consumer = gtk_button_new_with_label("+");
    g_signal_connect(WIDGETS->plus_consumer, "clicked", G_CALLBACK(push_consumer), my_data);
    gtk_container_add(GTK_CONTAINER(WIDGETS->consumer_container), WIDGETS->plus_consumer);

    sprintf(txt_number ,"%d",my_stack->current_consumers);
    WIDGETS->txt_consumer = gtk_label_new(txt_number);
    gtk_container_add(GTK_CONTAINER(WIDGETS->consumer_container), WIDGETS->txt_consumer);


    WIDGETS->less_consumer = gtk_button_new_with_label("-");
    g_signal_connect(WIDGETS->less_consumer, "clicked", G_CALLBACK(pop_consumer), my_data);
    gtk_container_add(GTK_CONTAINER(WIDGETS->consumer_container), WIDGETS->less_consumer);
}


static gboolean on_delete_event(GtkWidget *widget, gpointer user_data)
{
    // Detener la ejecución del bucle principal de eventos
    g_print("Cerrando window...");
    system("clear");
    gtk_main_quit();
    
    // Devolver FALSE para permitir que la señal "delete-event" continúe propagándose
    return FALSE;
}
void init_stack(ptr_stack stack){
    pthread_mutex_init(&stack->lock,NULL);
    pthread_cond_init(&stack->not_empty, NULL);
    pthread_cond_init(&stack->not_full, NULL);
    //g_mutex_init(&mutex);
    g_mutex_init(&mutex);
    g_cond_init(&not_empty);
    g_cond_init(&not_full);
    stack->current_stock=0;
    stack->current_consumers =0;
    stack->current_producers=0;
    stack->first_stack_consumer=NULL;
    stack->first_stack_producer=NULL;
}

void* consumir (void * arg){
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    ptr_callback_data data = (ptr_callback_data) arg;
    char txt_number[20];
    pthread_cleanup_push(cleanup_handler,&mutex);

    while(1){
        pthread_testcancel();
        
        if(g_mutex_trylock(&mutex)){
        while((data->my_stack->current_stock-CONSUMER_DECREMENT)/MAX_STOCK < 0.0) //Verifica que aun exista suficiente stock para su consumo
        {
            pthread_testcancel();
            g_print("CONSUMIDOR %lu: Esperando mas items...\n",pthread_self());
            g_cond_wait(&not_empty, &mutex); // Espera a una señal para despertar y verificar que ya puede consumir
        }
        data->my_stack->current_stock = data->my_stack->current_stock-CONSUMER_DECREMENT;
        g_print("\t\tConsumidor: %lu\t set_bar(%.2f)\n",pthread_self(), data->my_stack->current_stock/MAX_STOCK);
        if(MAX_STOCK-data->my_stack->current_stock >= PRODUCER_INCREMENT){
            g_cond_broadcast(&not_full); // Notifica que ya hay items
        }
        g_mutex_unlock(&mutex);
        sleep(1);        
        }

        
    }
    
    pthread_cleanup_pop(0);
    pthread_exit(NULL);
}
void* producir (void * arg){
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    ptr_callback_data data = (ptr_callback_data) arg;
    char txt_number[20];


    pthread_cleanup_push(cleanup_handler,&mutex);
    while(1){
        //pthread_mutex_lock(&data->my_stack->lock); // thread se queda en esta linea hasta que puede bloquear el mutex
        pthread_testcancel();
        if(g_mutex_trylock(&mutex)){
            while((data->my_stack->current_stock+PRODUCER_INCREMENT)/MAX_STOCK > 1.0){
            pthread_testcancel();
            g_print("PRODUCTOR %lu: Esperando consuman items...\n",pthread_self());
            g_cond_wait(&not_full, &mutex); // Espera a una señal para despertar y verificar que ya puede producir
            }
        data->my_stack->current_stock = data->my_stack->current_stock  + PRODUCER_INCREMENT;
        g_print("\t\tProductor: %lu\t set_bar(%.2f)\n",pthread_self(), data->my_stack->current_stock/MAX_STOCK);
        //gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(data->screen->bar_status), data->my_stack->current_stock/MAX_STOCK);
        sprintf(txt_number ,"%.0f",data->my_stack->current_stock);
        //gtk_label_set_text(GTK_LABEL(data->screen->txt_status), txt_number);
        if(data->my_stack->current_stock>=CONSUMER_DECREMENT){
            g_cond_broadcast(&not_empty); // Notifica que ya hay items
        }
        g_mutex_unlock(&mutex);
        //pthread_mutex_unlock(&data->my_stack->lock); // Desbloquea el mutex
        sleep(1);
        }
        
    }

    pthread_cleanup_pop(0);
    pthread_exit(NULL);
}
void  push_producer(GtkWidget* widget, gpointer DTO){
    char txt_number[20]; //variable auxiliar para parsear enteros a char[]
    ptr_callback_data data = (ptr_callback_data ) DTO;
    g_mutex_lock(&mutex);
    //pthread_mutex_lock(&data->my_stack->lock); // thread se queda en esta linea hasta que puede bloquear el mutex
    ptr_person aux = (ptr_person)malloc(sizeof(person));
    if(aux==NULL){
        g_print("ERROR: can not create new producer node...");
    }else{
        aux->next_person=data->my_stack->first_stack_producer;
        data->my_stack->first_stack_producer = aux;
        pthread_create(&data->my_stack->first_stack_producer->th_id,NULL,producir,data);
        data->my_stack->current_producers++;
        sprintf(txt_number ,"%d",data->my_stack->current_producers);
        gtk_label_set_text(GTK_LABEL(data->screen->txt_producer), txt_number);
        g_print("AÑADE PRODUCER %p - %p\n",data->my_stack->first_stack_producer, aux);
    }
    g_mutex_unlock(&mutex);
    //pthread_mutex_unlock(&data->my_stack->lock); // Desbloquea el mutex

}
void  pop_producer(GtkWidget* widget, gpointer DTO){
    ptr_callback_data data = (ptr_callback_data ) DTO;
    ptr_person aux;
    char txt_number[20];
    g_mutex_lock(&mutex);
    //pthread_mutex_lock(&data->my_stack->lock); // thread se queda en esta linea hasta que puede bloquear el mutex
    if(data->my_stack->first_stack_producer==NULL){
        g_print("EMPTY PRODUCER STACK\n");
    }else{
        g_print("Eliminando producer...\n");
        //move stack pointer
        aux = data->my_stack->first_stack_producer;
        data->my_stack->first_stack_producer = data->my_stack->first_stack_producer->next_person;
        data->my_stack->current_producers--;
        //update screen
        sprintf(txt_number ,"%d",data->my_stack->current_producers);
        gtk_label_set_text(GTK_LABEL(data->screen->txt_producer), txt_number);

        //kill thread
        pthread_cancel(aux->th_id);
        if(pthread_join(aux->th_id,NULL)!=0){
            g_print("ERROR pthread_join(%p)",aux);
        }else{
            g_print("PRODUCER: pthread_join(%p) success!\n", aux);
        }
        //free space
        free(aux);
        g_print("REMOVE PRODUCER, now first producer -> %p\n", data->my_stack->first_stack_producer);
    }
    g_mutex_unlock(&mutex);
    //pthread_mutex_unlock(&data->my_stack->lock); // Desbloquea el mutex
}
void  push_consumer(GtkWidget* widget, gpointer DTO){
    char txt_number[20]; //variable auxiliar para parsear enteros a char[]
    ptr_callback_data data = (ptr_callback_data ) DTO;
    g_mutex_lock(&mutex);
    //pthread_mutex_lock(&data->my_stack->lock); // thread se queda en esta linea hasta que puede bloquear el mutex
    ptr_person aux = (ptr_person)malloc(sizeof(person));

    if(aux==NULL){
        g_print("ERROR: can not create new consumer node...");
    }else{
        aux->next_person=data->my_stack->first_stack_consumer;
        data->my_stack->first_stack_consumer = aux;
        pthread_create(&aux->th_id,NULL,consumir,data);
        data->my_stack->current_consumers++;
        sprintf(txt_number ,"%d",data->my_stack->current_consumers);
        gtk_label_set_text(GTK_LABEL(data->screen->txt_consumer), txt_number);
        g_print("AÑADE CONSUMER %p - %p\n",data->my_stack->first_stack_consumer , aux);
    }

    g_mutex_unlock(&mutex);
    //pthread_mutex_unlock(&data->my_stack->lock); // Desbloquea el mutex
}
void  pop_consumer(GtkWidget* widget, gpointer DTO){
    ptr_callback_data data = (ptr_callback_data ) DTO;
    ptr_person aux;
    char txt_number[20];
    g_mutex_lock(&mutex);
    //pthread_mutex_lock(&data->my_stack->lock); // thread se queda en esta linea hasta que puede bloquear el mutex
    //g_mutex_lock(&gdk_threads_mutex);
    if(data->my_stack->first_stack_consumer==NULL){
        g_print("EMPTY CONSUMER STACK\n");
    }else{
        aux = data->my_stack->first_stack_consumer;
        data->my_stack->first_stack_consumer = data->my_stack->first_stack_consumer->next_person;
        data->my_stack->current_consumers--;
        sprintf(txt_number ,"%d",data->my_stack->current_consumers);
        gtk_label_set_text(GTK_LABEL(data->screen->txt_consumer), txt_number);
        //kill thread
        pthread_cancel(aux->th_id);
        if(pthread_join(aux->th_id,NULL)!=0){
            g_print("ERROR pthread_join(%p)",aux);
        }else{
            g_print("CUSTOMER: pthread_join(%p) success!\n", aux);
        }
        free(aux);
        g_print("REMOVE CONSUMER, now first consumer -> %p\n", data->my_stack->first_stack_consumer);
    }
    g_mutex_unlock(&mutex);
    //pthread_mutex_unlock(&data->my_stack->lock); // Desbloquea el mutex
}
void cleanup_handler(void* arg){
    //pthread_mutex_t* mut = (pthread_mutex_t*) arg;
    //g_mutex_unlock(&gdk_threads_mutex);
    //g_mutex_unlock(&mutex); // Desbloquea el mutex
}
gboolean update_bar(void * arg){
    ptr_callback_data data = (ptr_callback_data) arg;

    //g_mutex_lock(&mutex);
    char txt_number[20];
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(data->screen->bar_status), data->my_stack->current_stock/MAX_STOCK);
    sprintf(txt_number ,"%.0f",data->my_stack->current_stock);
    gtk_label_set_text(GTK_LABEL(data->screen->txt_status), txt_number);
    //g_mutex_unlock;
    sleep(.5);
    return TRUE;

}