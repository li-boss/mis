import request from './request';

export const submitInbound = (data) => request.post('/api/inventory/inbound', data);

export const fetchInventoryDashboard = () => request.get('/api/inventory/dashboard');
