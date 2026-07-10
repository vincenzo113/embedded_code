#ifndef INC_BUTTON_H_
#define INC_BUTTON_H_

#define BUTTON_INIT_STATE_OFF	(0)
#define BUTTON_INIT_STATE_ON	(1)
#define BUTTON_INIT_DELAY		(0)

#define PRESSED 				(GPIO_PIN_SET)
#define RELEASED 				(GPIO_PIN_RESET)

#define BUTTON_ERR		(-1)
#define BUTTON_OK		(0)

/**
 * @brief Tipo di dato per lo stato del pulsante, mappato sullo stato del pin GPIO.
 */
typedef GPIO_PinState button_state_t;

/**
 * @brief Struttura che rappresenta l'oggetto Pulsante.
 * @note Se modificata all'interno di una ISR, i campi di stato dovrebbero essere considerati volatili.
 */
struct button_s
{
	GPIO_TypeDef* GPIOx;       /**< Porta GPIO associata al pulsante (es. GPIOA, GPIOC) */
	uint16_t GPIO_Pin;         /**< Numero del pin GPIO (es. GPIO_PIN_13) */
	button_state_t state;      /**< Stato corrente del pulsante (fissato dall'ultimo campionamento o ISR) */
	button_state_t last_state; /**< Stato precedente del pin (usato per il debounce software) */
	uint32_t delay;            /**< Tempo di debounce/filtro in millisecondi */
	uint32_t last_tick;        /**< Ultimo valore di HAL_GetTick() in cui è stato rilevato un cambio di pin */
};

typedef struct button_s button_t;

/**
 * @brief Inizializza la struttura del pulsante con i parametri hardware e lo stato iniziale.
 * @note Questa funzione NON alloca la memoria per la struttura; l'allocazione deve essere gestita esternamente.
 * * @param button Puntatore alla struttura button_t da inizializzare.
 * @param GPIOx Porta GPIO hardware a cui è connesso il pulsante.
 * @param GPIO_Pin Pin GPIO hardware a cui è connesso il pulsante.
 * @param init_state Stato logico iniziale da assegnare al pulsante (BUTTON_INIT_STATE_OFF o ON).
 * @return int8_t BUTTON_OK in caso di successo, BUTTON_ERR se il puntatore a button è nullo.
 */
int8_t button_init(button_t* button, GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, button_state_t init_state);

/**
 * @brief Legge lo stato del pin hardware applicando un algoritmo di debounce software (polling).
 * @note Funziona correttamente solo se chiamata periodicamente nel ciclo principale e NON se si usa l'EXTI interrupt.
 * * @param button Puntatore alla struttura dell'istanza del pulsante.
 * @param state Puntatore alla variabile in cui verrà memorizzato lo stato filtrato risultante.
 * @return int8_t BUTTON_OK in caso di successo, BUTTON_ERR se i parametri sono nulli.
 */
int8_t button_read(button_t* button, button_state_t* state);

/**
 * @brief Imposta il tempo di ritardo (debounce) per la lettura software del pulsante.
 * * @param button Puntatore alla struttura dell'istanza del pulsante.
 * @param read_delay Tempo di ritardo espresso in millisecondi.
 * @return int8_t BUTTON_OK in caso di successo, BUTTON_ERR se il puntatore è nullo.
 */
int8_t button_set_delay(button_t* button, uint32_t read_delay);

/**
 * @brief Recupera il tempo di ritardo (debounce) configurato per il pulsante.
 * * @param button Puntatore alla struttura dell'istanza del pulsante.
 * @param read_delay Puntatore alla variabile che riceverà il valore del delay in millisecondi.
 * @return int8_t BUTTON_OK in caso di successo, BUTTON_ERR se i parametri sono nulli.
 */
int8_t button_get_delay(button_t* button, uint32_t* read_delay);

/**
 * @brief Restituisce il puntatore alla porta GPIO hardware (GPIO_TypeDef) utilizzata dal pulsante.
 * * @param button Puntatore costante alla struttura dell'istanza del pulsante.
 * @param port Puntatore a un puntatore GPIO_TypeDef che riceverà l'indirizzo della porta.
 * @return int8_t BUTTON_OK in caso di successo, BUTTON_ERR se i parametri sono nulli.
 */
int8_t button_get_port(const button_t* button, GPIO_TypeDef** port);

/**
 * @brief Restituisce il numero del pin GPIO hardware utilizzato dal pulsante.
 * * @param button Puntatore costante alla struttura dell'istanza del pulsante.
 * @param pin Puntatore alla variabile uint16_t che riceverà il numero del pin.
 * @return int8_t BUTTON_OK in caso di successo, BUTTON_ERR se i parametri sono nulli.
 */
int8_t button_get_pin_number(const button_t* button, uint16_t* pin);

/**
 * @brief Legge l'ultimo stato memorizzato nella struttura (correntemente usato dall'architettura FSM).
 * @note ATTENZIONE: Soffre di race condition se usata in combinazione asincrona con button_reset_pressed.
 * * @param button Puntatore alla struttura dell'istanza del pulsante.
 * @param state Puntatore alla variabile che riceverà lo stato interno corrente.
 * @return int8_t BUTTON_OK in caso di successo, BUTTON_ERR se il puntatore è nullo.
 */
int8_t button_read_pressed(button_t* button, button_state_t* state);

/**
 * @brief Ripristina lo stato del pulsante a RELEASED (rilasciato).
 * @note Tipicamente chiamata dalla FSM dopo aver consumato l'evento di pressione.
 * * @param button Puntatore alla struttura dell'istanza del pulsante.
 * @return int8_t BUTTON_OK in caso di successo, BUTTON_ERR se il puntatore è nullo.
 */
int8_t button_reset_pressed(button_t* button);

/**
 * @brief Gestore dell'evento di pressione da invocare all'interno delle ISR (Interrupt Service Routine).
 * @details Verifica se il pin che ha scatenato l'interrupt coincide con quello del pulsante e, in tal caso, ne forza lo stato a PRESSED.
 * * @param button Puntatore alla struttura dell'istanza del pulsante.
 * @param pin Numero del pin GPIO che ha generato l'interrupt EXTI.
 * @return int8_t BUTTON_OK in caso di successo, BUTTON_ERR se il puntatore è nullo.
 */
int8_t button_pressed_handler(button_t* button, uint16_t pin);

/**
 * @brief Legge e resetta lo stato di pressione del pulsante in modo atomico.
 * * @details Questa funzione copia lo stato corrente del pulsante nella variabile di output
 * e contestualmente ne resetta lo stato a RELEASED. L'intera operazione viene
 * eseguita all'interno di una sezione critica (disabilitando temporaneamente gli
 * interrupt) per prevenire race condition qualora lo stato del pulsante venga
 * aggiornato asincronamente all'interno di una ISR (Interrupt Service Routine).
 * * @pre Il pulsante deve essere stato precedentemente inizializzato con @ref button_init.
 * * @param[in]  button Puntatore alla struttura dell'istanza del pulsante (button_t).
 * @param[out] state  Puntatore alla variabile che riceverà lo stato catturato prima del reset.
 * * @retval  BUTTON_OK  Operazione completata con successo, stato letto e azzerato.
 * @retval  BUTTON_ERR Uno o entrambi i puntatori passati come argomento sono NULL.
 * * @note Da utilizzare nel thread principale (es. nella FSM) in sostituzione della
 * coppia di funzioni non protette button_read_pressed e button_reset_pressed.
 */
int8_t button_get_and_clear_pressed(button_t* button, button_state_t* state);


#endif /* INC_BUTTON_H_ */
