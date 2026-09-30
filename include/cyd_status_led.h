#pragma once

// Vendor board RGB status LED (GPIO40): start a low-brightness rainbow cycle
// and advance it without blocking CAN, display, or web service work.
void cydStatusLedBegin();
void cydStatusLedUpdate();
