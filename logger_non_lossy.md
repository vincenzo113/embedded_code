## Come si integra un logger non lossy nella FSM? 
Per usare un logger non lossy occorre che utilizziamo la queue.c e nel main 
```c
int main(){
    if(FSM_step() != FSM_ERR) return -1; 
    uart_tx_task(); //consuma un elemento inserito nella coda 
  
}
```

Quindi riprendiamo l'esercizio della coda. Nella parte del while mettiamo come sopra. Alla macchina a stati va passato come parametro la coda.Quindi nel main allochiamo la memoria necessaria per la coda e la passiamo alla FSM, la init della FSM quindi dovrá prevedere la coda e quindi anche nella struttura FSM_t
```c
FSM_init(queue_t * queue , ... );
```

quindi quando vorremo trasmettere andremo ad accodare il messaggio nella coda e uart_tx_task andrá a consumare il messaggio con la logica FIFO