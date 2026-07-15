# Execution Log

## Profile A
* **delay_ms: 60** | Miss Rate: 0.33% | Overhead: 1.55x | Result: VALID
* **Notes:** Baseline test with 2:1 XOR FEC architecture.

## Profile B
* **delay_ms: 60** | Miss Rate: 29.47% | Overhead: 1.55x | Result: INVALID
* **Notes:** High burst loss breaks FEC at this low delay.
* **delay_ms: 150** | Miss Rate: 0.87% | Overhead: 1.55x | Result: VALID
* **Notes:** Generous delay allows successful playout.
* **delay_ms: 100** | Miss Rate: 0.93% | Overhead: 1.55x | Result: VALID
* **Notes:** Binary searching for optimal delay.